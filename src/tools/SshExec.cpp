#include "tools/SshExec.h"
#include <libssh2.h>

#if !defined(_WIN32)
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#endif

namespace macxterm::tools {

SshExec::~SshExec() { disconnectSession(); }

#if !defined(_WIN32)
bool SshExec::connectSession(const core::Session& s) {
    disconnectSession();
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(s.host().toUtf8().constData(),
                    QByteArray::number(s.port()).constData(), &hints, &res) != 0)
        return false;
    int fd = -1;
    for (auto* p = res; p; p = p->ai_next) {
        fd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;
        if (::connect(fd, p->ai_addr, p->ai_addrlen) == 0) break;
        ::close(fd); fd = -1;
    }
    freeaddrinfo(res);
    if (fd < 0) return false;

    LIBSSH2_SESSION* sess = libssh2_session_init();
    if (!sess) { ::close(fd); return false; }
    libssh2_session_set_blocking(sess, 1);
    if (libssh2_session_handshake(sess, fd) != 0) {
        libssh2_session_free(sess); ::close(fd); return false;
    }
    const QByteArray user = s.username().toUtf8();
    const QByteArray key = s.param("keyfile").toUtf8();
    bool authed;
    if (!key.isEmpty()) {
        const QByteArray pass = s.param("passphrase").toUtf8();
        authed = libssh2_userauth_publickey_fromfile(sess, user.constData(), nullptr,
                                                     key.constData(), pass.constData()) == 0;
    } else {
        const QByteArray pw = s.param("password").toUtf8();
        authed = libssh2_userauth_password(sess, user.constData(), pw.constData()) == 0;
    }
    if (!authed) { libssh2_session_disconnect(sess, "bye"); libssh2_session_free(sess); ::close(fd); return false; }
    m_session = sess; m_sock = fd;
    return true;
}
#else
bool SshExec::connectSession(const core::Session&) { return false; }
#endif

QByteArray SshExec::run(const QString& command) {
    if (!m_session) return {};
    LIBSSH2_CHANNEL* chan = libssh2_channel_open_session(m_session);
    if (!chan) return {};
    QByteArray out;
    if (libssh2_channel_exec(chan, command.toUtf8().constData()) == 0) {
        char buf[8192];
        ssize_t n;
        while ((n = libssh2_channel_read(chan, buf, sizeof(buf))) > 0)
            out.append(buf, static_cast<int>(n));
    }
    libssh2_channel_close(chan);
    libssh2_channel_free(chan);
    return out;
}

void SshExec::disconnectSession() {
    if (m_session) {
        libssh2_session_disconnect(m_session, "bye");
        libssh2_session_free(m_session);
        m_session = nullptr;
    }
#if !defined(_WIN32)
    if (m_sock >= 0) { ::close(m_sock); m_sock = -1; }
#endif
}

} // namespace macxterm::tools
