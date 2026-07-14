#include "connect/SshConnection.h"
#include <QSocketNotifier>
#include <libssh2.h>

#if !defined(_WIN32)
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#endif

namespace macxterm::connect {

SshConnection::SshConnection(QObject* parent) : IConnection(parent) {
    libssh2_init(0);
}

SshConnection::~SshConnection() {
    cleanup();
    libssh2_exit();
}

#if defined(_WIN32)
// TODO(Phase 3): Winsock transport. Header stays cross-platform.
bool SshConnection::connectSession(const core::Session&) {
    setState(State::Failed);
    emit errorOccurred("SSH on Windows not yet implemented");
    return false;
}
bool SshConnection::doHandshakeAndAuth(const core::Session&) { return false; }
#else

static int openSocket(const QByteArray& host, int port) {
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    const QByteArray portStr = QByteArray::number(port);
    if (getaddrinfo(host.constData(), portStr.constData(), &hints, &res) != 0) return -1;
    int fd = -1;
    for (auto* p = res; p; p = p->ai_next) {
        fd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;
        if (::connect(fd, p->ai_addr, p->ai_addrlen) == 0) break;
        ::close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    return fd;
}

bool SshConnection::doHandshakeAndAuth(const core::Session& session) {
    if (libssh2_session_handshake(m_session, m_sock) != 0) {
        emit errorOccurred("SSH handshake failed");
        return false;
    }
    const QByteArray user = session.username().toUtf8();
    const QByteArray keyfile = session.param("keyfile").toUtf8();

    if (!keyfile.isEmpty()) {
        const QByteArray pass = session.param("passphrase").toUtf8();
        if (libssh2_userauth_publickey_fromfile(
                m_session, user.constData(), nullptr,
                keyfile.constData(), pass.constData()) != 0) {
            emit errorOccurred("Public-key authentication failed");
            return false;
        }
    } else {
        const QByteArray pass = session.param("password").toUtf8();
        if (libssh2_userauth_password(m_session, user.constData(), pass.constData()) != 0) {
            emit errorOccurred("Password authentication failed");
            return false;
        }
    }
    return true;
}

bool SshConnection::connectSession(const core::Session& session) {
    setState(State::Connecting);

    m_sock = openSocket(session.host().toUtf8(), session.port());
    if (m_sock < 0) {
        setState(State::Failed);
        emit errorOccurred("Failed to connect socket");
        return false;
    }

    m_session = libssh2_session_init();
    if (!m_session) { setState(State::Failed); return false; }
    libssh2_session_set_blocking(m_session, 1);

    if (!doHandshakeAndAuth(session)) { cleanup(); setState(State::Failed); return false; }

    m_channel = libssh2_channel_open_session(m_session);
    if (!m_channel) { cleanup(); setState(State::Failed); return false; }

    libssh2_channel_request_pty_ex(m_channel, "xterm-256color", 14, nullptr, 0,
                                   m_cols, m_rows, 0, 0);

    // Optional X11 forwarding (Architecture §6.3 / research §1.3).
    if (session.param("x11", "1") != "0") {
        libssh2_channel_x11_req(m_channel, 0);
    }

    if (libssh2_channel_shell(m_channel) != 0) {
        cleanup(); setState(State::Failed);
        emit errorOccurred("Failed to open shell channel");
        return false;
    }

    // Switch to non-blocking and drive reads via the Qt event loop.
    libssh2_session_set_blocking(m_session, 0);
    fcntl(m_sock, F_SETFL, O_NONBLOCK);
    m_notifier = new QSocketNotifier(m_sock, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &SshConnection::onSocketReadable);

    setState(State::Connected);
    return true;
}
#endif // !_WIN32

void SshConnection::onSocketReadable() {
    if (!m_channel) return;
    char buf[4096];
    for (;;) {
        const ssize_t n = libssh2_channel_read(m_channel, buf, sizeof(buf));
        if (n > 0) {
            emit dataReceived(QByteArray(buf, static_cast<int>(n)));
        } else if (n == LIBSSH2_ERROR_EAGAIN) {
            break;
        } else {
            break;
        }
    }
    if (libssh2_channel_eof(m_channel)) {
        disconnectSession();
        setState(State::Closed);
    }
}

qint64 SshConnection::send(const QByteArray& data) {
    if (!m_channel) return -1;
    return libssh2_channel_write(m_channel, data.constData(), data.size());
}

void SshConnection::resize(int cols, int rows) {
    m_cols = cols; m_rows = rows;
    if (m_channel) libssh2_channel_request_pty_size(m_channel, cols, rows);
}

void SshConnection::disconnectSession() {
    cleanup();
    setState(State::Disconnected);
}

void SshConnection::cleanup() {
    if (m_notifier) { m_notifier->setEnabled(false); m_notifier->deleteLater(); m_notifier = nullptr; }
    if (m_channel) { libssh2_channel_free(m_channel); m_channel = nullptr; }
    if (m_session) {
        libssh2_session_disconnect(m_session, "bye");
        libssh2_session_free(m_session);
        m_session = nullptr;
    }
#if !defined(_WIN32)
    if (m_sock >= 0) { ::close(m_sock); m_sock = -1; }
#endif
}

} // namespace macxterm::connect
