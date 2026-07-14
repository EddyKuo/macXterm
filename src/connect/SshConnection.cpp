#include "connect/SshConnection.h"
#include <QSocketNotifier>
#include <QString>
#include <QByteArray>
#include <libssh2.h>
#include <cstdio>

#if !defined(_WIN32)
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cstdlib>
#endif

namespace macxterm::connect {

#if !defined(_WIN32)
namespace {
// Connect to the *local* X server (where forwarded X apps should appear), based
// on $DISPLAY. Handles a unix-socket DISPLAY path, ":N" (local unix socket), and
// "host:N" (TCP 6000+N). Returns a connected fd or -1.
int connectLocalXServer() {
    const char* disp = std::getenv("DISPLAY");
    if (!disp || !*disp) return -1;
    QString d = QString::fromLocal8Bit(disp);

    // A DISPLAY that is an absolute path is itself a unix socket (macOS XQuartz).
    if (d.startsWith('/')) {
        int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) return -1;
        sockaddr_un addr{}; addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, d.toLocal8Bit().constData(), sizeof(addr.sun_path) - 1);
        if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) return fd;
        ::close(fd); return -1;
    }
    const int colon = d.lastIndexOf(':');
    if (colon < 0) return -1;
    const QString host = d.left(colon);
    const int display = d.mid(colon + 1).section('.', 0, 0).toInt();

    if (host.isEmpty() || host == "unix") {
        int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) return -1;
        sockaddr_un addr{}; addr.sun_family = AF_UNIX;
        std::snprintf(addr.sun_path, sizeof(addr.sun_path), "/tmp/.X11-unix/X%d", display);
        if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) return fd;
        ::close(fd); return -1;
    }
    // TCP: host:(6000+display)
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct addrinfo hints{}, *res = nullptr; hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host.toLocal8Bit().constData(),
                    QByteArray::number(6000 + display).constData(), &hints, &res) != 0) {
        ::close(fd); return -1;
    }
    const bool ok = res && ::connect(fd, res->ai_addr, res->ai_addrlen) == 0;
    if (res) freeaddrinfo(res);
    if (!ok) { ::close(fd); return -1; }
    return fd;
}
} // namespace

// libssh2 X11 open callback → hand off to the SshConnection instance.
static void ssh_x11_cb(LIBSSH2_SESSION*, LIBSSH2_CHANNEL* channel,
                       char* /*shost*/, int /*sport*/, void** abstract) {
    if (abstract && *abstract)
        static_cast<SshConnection*>(*abstract)->acceptX11(channel);
}
#endif

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

    // Pass `this` as the session abstract so the X11 callback can reach us.
    m_session = libssh2_session_init_ex(nullptr, nullptr, nullptr, this);
    if (!m_session) { setState(State::Failed); return false; }
    libssh2_session_set_blocking(m_session, 1);
    libssh2_session_callback_set(m_session, LIBSSH2_CALLBACK_X11,
                                 reinterpret_cast<void*>(&ssh_x11_cb));

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
    pumpX11();   // service forwarded X11 channels on the same session
    if (libssh2_channel_eof(m_channel)) {
        disconnectSession();
        setState(State::Closed);
    }
}

#if !defined(_WIN32)
void SshConnection::acceptX11(LIBSSH2_CHANNEL* channel) {
    const int xfd = connectLocalXServer();
    if (xfd < 0) {
        emit errorOccurred(QStringLiteral("X11 forwarding: no local X server ($DISPLAY)"));
        libssh2_channel_free(channel);
        return;
    }
    fcntl(xfd, F_SETFL, O_NONBLOCK);
    auto* n = new QSocketNotifier(xfd, QSocketNotifier::Read, this);
    connect(n, &QSocketNotifier::activated, this, &SshConnection::onX11SocketReadable);
    m_x11.push_back({channel, xfd, n});
}

void SshConnection::onX11SocketReadable() { pumpX11(); }

void SshConnection::pumpX11() {
    char buf[8192];
    for (auto it = m_x11.begin(); it != m_x11.end();) {
        bool dead = false;
        // local X → SSH channel
        for (;;) {
            const ssize_t r = ::recv(it->xsock, buf, sizeof(buf), 0);
            if (r > 0) {
                int off = 0;
                while (off < r) {
                    const ssize_t w = libssh2_channel_write(it->chan, buf + off, r - off);
                    if (w == LIBSSH2_ERROR_EAGAIN) continue;
                    if (w < 0) { dead = true; break; }
                    off += w;
                }
            } else if (r == 0) { dead = true; break; }
            else break;   // EAGAIN / no data
            if (dead) break;
        }
        // SSH channel → local X
        for (;;) {
            const ssize_t r = libssh2_channel_read(it->chan, buf, sizeof(buf));
            if (r > 0) ::send(it->xsock, buf, r, 0);
            else break;
        }
        if (dead || libssh2_channel_eof(it->chan)) {
            it->notifier->setEnabled(false); it->notifier->deleteLater();
            ::close(it->xsock);
            libssh2_channel_free(it->chan);
            it = m_x11.erase(it);
        } else {
            ++it;
        }
    }
}
#else
void SshConnection::acceptX11(LIBSSH2_CHANNEL*) {}
void SshConnection::onX11SocketReadable() {}
void SshConnection::pumpX11() {}
#endif

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
#if !defined(_WIN32)
    for (auto& f : m_x11) {
        if (f.notifier) { f.notifier->setEnabled(false); f.notifier->deleteLater(); }
        if (f.xsock >= 0) ::close(f.xsock);
        if (f.chan) libssh2_channel_free(f.chan);
    }
    m_x11.clear();
#endif
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
