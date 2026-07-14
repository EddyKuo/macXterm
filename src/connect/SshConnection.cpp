#include "connect/SshConnection.h"
#include <QSocketNotifier>
#include <QString>
#include <QByteArray>
#include <libssh2.h>
#include <cstdio>
#include <thread>
#include <atomic>

#if !defined(_WIN32)
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cstdlib>
#include <ctime>
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

// SSH-over-SSH gateway state: a gateway session whose direct-tcpip channel to
// the target is bridged to a local socketpair; the target session then runs its
// own handshake over that socketpair fd. A pump thread moves bytes between the
// socketpair and the gateway channel.
struct SshConnection::JumpRelay {
    LIBSSH2_SESSION* gwSession = nullptr;
    LIBSSH2_CHANNEL* gwChannel = nullptr;
    int gwSock = -1;
    int pumpEnd = -1;               // socketpair end serviced by the pump thread
    std::atomic<bool> stop{false};
    std::thread pump;
};

int SshConnection::openViaJump(const core::Session& session) {
    // Gateway spec: "[user@]host[:port]" in param "gateway". Empty → no jump.
    QString spec = session.param("gateway");
    if (spec.isEmpty()) return -1;

    QString gwUser = session.username();
    if (const int at = spec.indexOf('@'); at >= 0) { gwUser = spec.left(at); spec = spec.mid(at + 1); }
    QString gwHost = spec;
    int gwPort = 22;
    if (const int col = spec.indexOf(':'); col >= 0) {
        gwHost = spec.left(col);
        gwPort = spec.mid(col + 1).toInt();
        if (gwPort <= 0) gwPort = 22;
    }
    if (!session.param("gateway_user").isEmpty()) gwUser = session.param("gateway_user");

    const int gwSock = openSocket(gwHost.toUtf8(), gwPort);
    if (gwSock < 0) { emit errorOccurred("Gateway: connect failed"); return -1; }
    LIBSSH2_SESSION* gw = libssh2_session_init();
    if (!gw) { ::close(gwSock); return -1; }
    libssh2_session_set_blocking(gw, 1);
    if (libssh2_session_handshake(gw, gwSock) != 0) {
        emit errorOccurred("Gateway: SSH handshake failed");
        libssh2_session_free(gw); ::close(gwSock); return -1;
    }
    // Gateway auth: prefer gateway_* creds, else fall back to the target's.
    const QByteArray gu = gwUser.toUtf8();
    const QByteArray gkey = session.param("gateway_keyfile").toUtf8();
    bool authed;
    if (!gkey.isEmpty()) {
        const QByteArray gpass = session.param("gateway_passphrase").toUtf8();
        authed = libssh2_userauth_publickey_fromfile(gw, gu.constData(), nullptr,
                                                     gkey.constData(), gpass.constData()) == 0;
    } else {
        QByteArray gpw = session.param("gateway_password").toUtf8();
        if (gpw.isEmpty()) gpw = session.param("password").toUtf8();
        authed = libssh2_userauth_password(gw, gu.constData(), gpw.constData()) == 0;
    }
    if (!authed) {
        emit errorOccurred("Gateway: authentication failed");
        libssh2_session_disconnect(gw, "bye"); libssh2_session_free(gw); ::close(gwSock);
        return -1;
    }
    // Open a direct-tcpip channel from the gateway to the real target.
    LIBSSH2_CHANNEL* ch = libssh2_channel_direct_tcpip_ex(
        gw, session.host().toUtf8().constData(), session.port(), "127.0.0.1", 0);
    if (!ch) {
        emit errorOccurred("Gateway: could not open channel to target");
        libssh2_session_disconnect(gw, "bye"); libssh2_session_free(gw); ::close(gwSock);
        return -1;
    }
    // Bridge the channel to a socketpair; hand one end to the target session.
    int sp[2];
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sp) != 0) {
        libssh2_channel_free(ch);
        libssh2_session_disconnect(gw, "bye"); libssh2_session_free(gw); ::close(gwSock);
        return -1;
    }
    libssh2_session_set_blocking(gw, 0);
    m_jump = new JumpRelay;
    m_jump->gwSession = gw;
    m_jump->gwChannel = ch;
    m_jump->gwSock = gwSock;
    m_jump->pumpEnd = sp[1];
    JumpRelay* jr = m_jump;
    jr->pump = std::thread([jr] {
        char buf[16384];
        ::fcntl(jr->pumpEnd, F_SETFL, O_NONBLOCK);
        while (!jr->stop.load()) {
            bool idle = true;
            // socketpair -> channel
            const ssize_t r = ::recv(jr->pumpEnd, buf, sizeof(buf), 0);
            if (r > 0) {
                idle = false;
                int off = 0;
                while (off < r) {
                    const ssize_t w = libssh2_channel_write(jr->gwChannel, buf + off, r - off);
                    if (w == LIBSSH2_ERROR_EAGAIN) continue;
                    if (w < 0) break;
                    off += w;
                }
            } else if (r == 0) { break; }
            // channel -> socketpair
            const ssize_t n = libssh2_channel_read(jr->gwChannel, buf, sizeof(buf));
            if (n > 0) { idle = false; ::send(jr->pumpEnd, buf, n, 0); }
            else if (n == 0 && libssh2_channel_eof(jr->gwChannel)) break;
            if (idle) { struct timespec ts{0, 3'000'000}; nanosleep(&ts, nullptr); }
        }
    });
    return sp[0];   // target session's socket fd
}

// libssh2 keyboard-interactive callback: answer every prompt with the stored
// password (the common single-prompt case). Reaches the SshConnection via the
// session abstract set at init.
static void kbd_callback(const char*, int, const char*, int,
                         int num_prompts, const LIBSSH2_USERAUTH_KBDINT_PROMPT*,
                         LIBSSH2_USERAUTH_KBDINT_RESPONSE* responses, void** abstract) {
    auto* self = abstract ? static_cast<SshConnection*>(*abstract) : nullptr;
    const QByteArray pw = self ? self->kbdPassword() : QByteArray();
    for (int i = 0; i < num_prompts; ++i) {
        responses[i].text = static_cast<char*>(malloc(pw.size()));
        if (responses[i].text) { memcpy(responses[i].text, pw.constData(), pw.size()); }
        responses[i].length = static_cast<unsigned int>(pw.size());
    }
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
        return true;
    }

    // Try the local SSH agent first when requested (param "agent" = "1").
    if (session.param("agent") == "1") {
        LIBSSH2_AGENT* agent = libssh2_agent_init(m_session);
        bool agentOk = false;
        if (agent && libssh2_agent_connect(agent) == 0 && libssh2_agent_list_identities(agent) == 0) {
            struct libssh2_agent_publickey* id = nullptr;
            while (libssh2_agent_get_identity(agent, &id, id) == 0) {
                if (libssh2_agent_userauth(agent, user.constData(), id) == 0) { agentOk = true; break; }
            }
        }
        if (agent) { libssh2_agent_disconnect(agent); libssh2_agent_free(agent); }
        if (agentOk) return true;
    }

    const QByteArray pass = session.param("password").toUtf8();
    if (libssh2_userauth_password(m_session, user.constData(), pass.constData()) == 0)
        return true;

    // Fall back to keyboard-interactive (answering prompts with the password).
    m_kbdPassword = pass;
    if (libssh2_userauth_keyboard_interactive(m_session, user.constData(), &kbd_callback) == 0)
        return true;

    emit errorOccurred("Authentication failed");
    return false;
}

bool SshConnection::connectSession(const core::Session& session) {
    setState(State::Connecting);

    // If a gateway/jump host is configured, tunnel through it; otherwise dial the
    // target directly.
    if (!session.param("gateway").isEmpty()) {
        m_sock = openViaJump(session);
        if (m_sock < 0) { setState(State::Failed); return false; }
    } else {
        m_sock = openSocket(session.host().toUtf8(), session.port());
    }
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
    // Optional transport compression (must be requested before the handshake).
    if (session.param("compression") == "1")
        libssh2_session_flag(m_session, LIBSSH2_FLAG_COMPRESS, 1);

    if (!doHandshakeAndAuth(session)) { cleanup(); setState(State::Failed); return false; }

    m_channel = libssh2_channel_open_session(m_session);
    if (!m_channel) { cleanup(); setState(State::Failed); return false; }

    libssh2_channel_request_pty_ex(m_channel, "xterm-256color", 14, nullptr, 0,
                                   m_cols, m_rows, 0, 0);

    // Optional X11 forwarding (Architecture §6.3 / research §1.3).
    if (session.param("x11", "1") != "0") {
        libssh2_channel_x11_req(m_channel, 0);
    }
    // Optional SSH agent forwarding (param "agentforward" = "1").
    if (session.param("agentforward") == "1") {
        libssh2_channel_request_auth_agent(m_channel);
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
    if (m_jump) {
        m_jump->stop.store(true);
        if (m_jump->pump.joinable()) m_jump->pump.join();
        if (m_jump->gwChannel) libssh2_channel_free(m_jump->gwChannel);
        if (m_jump->gwSession) {
            libssh2_session_disconnect(m_jump->gwSession, "bye");
            libssh2_session_free(m_jump->gwSession);
        }
        if (m_jump->pumpEnd >= 0) ::close(m_jump->pumpEnd);
        if (m_jump->gwSock >= 0) ::close(m_jump->gwSock);
        delete m_jump;
        m_jump = nullptr;
    }
#endif
}

} // namespace macxterm::connect
