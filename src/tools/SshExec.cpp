#include "tools/SshExec.h"
#include "platform/Net.h"
#include <libssh2.h>
#if !defined(_WIN32)
#include <sys/types.h>   // ssize_t (used by run(); was previously via <unistd.h>)
#endif

namespace macxterm::tools {

SshExec::~SshExec() { disconnectSession(); }

// Socket transport via the platform net shim (Winsock on Windows, BSD sockets on
// Unix — identical behaviour on Unix). connectTcp mirrors the old inline
// getaddrinfo/socket/connect loop (AF_UNSPEC, first working address wins).
bool SshExec::connectSession(const core::Session& s) {
    disconnectSession();
    const int fd = platform::net::connectTcp(s.host().toUtf8().constData(), s.port());
    if (fd < 0) return false;

    LIBSSH2_SESSION* sess = libssh2_session_init();
    if (!sess) { platform::net::closeSocket(fd); return false; }
    libssh2_session_set_blocking(sess, 1);
    if (libssh2_session_handshake(sess, fd) != 0) {
        libssh2_session_free(sess); platform::net::closeSocket(fd); return false;
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
    if (!authed) { libssh2_session_disconnect(sess, "bye"); libssh2_session_free(sess); platform::net::closeSocket(fd); return false; }
    m_session = sess; m_sock = fd;
    return true;
}

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
    if (m_sock >= 0) { platform::net::closeSocket(m_sock); m_sock = -1; }
}

} // namespace macxterm::tools
