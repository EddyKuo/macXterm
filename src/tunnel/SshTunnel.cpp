#include "tunnel/SshTunnel.h"
#include "tunnel/Socks.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <libssh2.h>
#include <functional>
#include <vector>
#include "platform/Net.h"

#if defined(_WIN32)
#include <winsock2.h>
#include <basetsd.h>
using ssize_t = SSIZE_T;   // POSIX type used by the shared recv/send relay loops
#else
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

namespace macxterm::tunnel {

SshTunnel::SshTunnel(QObject* parent) : QObject(parent) { libssh2_init(0); }
SshTunnel::~SshTunnel() { stop(); libssh2_exit(); }

bool SshTunnel::isRunning() const {
    return (m_server && m_server->isListening()) || (m_remoteListener != nullptr);
}
quint16 SshTunnel::listenPort() const { return m_server ? m_server->serverPort() : 0; }

namespace {
// Open a raw TCP socket to host:port (blocking) via the platform net shim
// (Winsock on Windows / BSD sockets on Unix — identical on Unix). Returns fd/-1.
int dialTcp(const QByteArray& host, int port) {
    return platform::net::connectTcp(host.constData(), port);
}

// Authenticate a freshly-handshaken session with the Session's credentials.
bool authSession(LIBSSH2_SESSION* sess, const core::Session& srv) {
    const QByteArray user = srv.username().toUtf8();
    const QByteArray key = srv.param("keyfile").toUtf8();
    if (!key.isEmpty()) {
        const QByteArray pass = srv.param("passphrase").toUtf8();
        return libssh2_userauth_publickey_fromfile(sess, user.constData(), nullptr,
                                                    key.constData(), pass.constData()) == 0;
    }
    const QByteArray pw = srv.param("password").toUtf8();
    return libssh2_userauth_password(sess, user.constData(), pw.constData()) == 0;
}

// Pump bytes both directions between a client fd and a (non-blocking) SSH
// channel until either side closes.
void pumpRelay(int clientFd, LIBSSH2_CHANNEL* chan, const std::atomic<bool>* stopping) {
    char buf[16384];
    while (!stopping->load()) {
        if (platform::net::pollReadable(clientFd, 20) > 0) {
            const ssize_t n = ::recv(clientFd, buf, sizeof(buf), 0);
            if (n <= 0) break;
            int off = 0;
            while (off < n) {
                const ssize_t w = libssh2_channel_write(chan, buf + off, n - off);
                if (w == LIBSSH2_ERROR_EAGAIN) continue;
                if (w < 0) { off = -1; break; }
                off += w;
            }
            if (off < 0) break;
        }
        bool closed = false;
        for (;;) {
            const ssize_t n = libssh2_channel_read(chan, buf, sizeof(buf));
            if (n == LIBSSH2_ERROR_EAGAIN) break;
            if (n <= 0) { closed = true; break; }
            ::send(clientFd, buf, n, 0);
        }
        if (closed || libssh2_channel_eof(chan)) break;
    }
}

// Per-client worker for Local/Dynamic tunnels: open a dedicated SSH session,
// (for Dynamic) negotiate SOCKS to learn the target, open a direct-tcpip
// channel and relay.
void relayWorker(core::Session sshServer, Tunnel tunnel, int clientFd,
                 const std::atomic<bool>* stopping) {
    QByteArray targetHost = tunnel.targetHost.toUtf8();
    int targetPort = tunnel.targetPort;
    if (tunnel.kind == TunnelKind::Dynamic) {
        if (!socksNegotiate(clientFd, targetHost, targetPort)) { platform::net::closeSocket(clientFd); return; }
    }
    const int sshFd = dialTcp(sshServer.host().toUtf8(), sshServer.port());
    if (sshFd < 0) { platform::net::closeSocket(clientFd); return; }
    LIBSSH2_SESSION* sess = libssh2_session_init();
    if (!sess) { platform::net::closeSocket(sshFd); platform::net::closeSocket(clientFd); return; }
    libssh2_session_set_blocking(sess, 1);
    bool ok = libssh2_session_handshake(sess, sshFd) == 0 && authSession(sess, sshServer);
    LIBSSH2_CHANNEL* chan = ok
        ? libssh2_channel_direct_tcpip_ex(sess, targetHost.constData(), targetPort, "127.0.0.1", 0)
        : nullptr;
    if (chan) {
        libssh2_session_set_blocking(sess, 0);
        pumpRelay(clientFd, chan, stopping);
        libssh2_channel_free(chan);
    }
    if (sess) { libssh2_session_disconnect(sess, "bye"); libssh2_session_free(sess); }
    platform::net::closeSocket(sshFd);
    platform::net::closeSocket(clientFd);
}

// Single-threaded listener for Remote (-R) tunnels. libssh2 sessions are not
// thread-safe, so one session multiplexes the forward listener + every accepted
// channel and its locally-dialed socket in a single poll loop.
void remoteListenerWorker(core::Session sshServer, Tunnel tunnel,
                          const std::atomic<bool>* stopping,
                          std::function<void(QString)> report) {
    const int sshFd = dialTcp(sshServer.host().toUtf8(), sshServer.port());
    if (sshFd < 0) { report(QStringLiteral("Remote tunnel: SSH connect failed")); return; }
    LIBSSH2_SESSION* sess = libssh2_session_init();
    if (!sess) { platform::net::closeSocket(sshFd); return; }
    libssh2_session_set_blocking(sess, 1);
    if (!(libssh2_session_handshake(sess, sshFd) == 0 && authSession(sess, sshServer))) {
        report(QStringLiteral("Remote tunnel: SSH auth failed"));
        libssh2_session_free(sess); platform::net::closeSocket(sshFd); return;
    }
    int boundPort = 0;
    const QByteArray bindAddr = tunnel.bindAddr.toUtf8();
    LIBSSH2_LISTENER* listener = libssh2_channel_forward_listen_ex(
        sess, bindAddr.isEmpty() ? nullptr : bindAddr.constData(),
        tunnel.bindPort, &boundPort, 16);
    if (!listener) {
        report(QStringLiteral("Remote tunnel: server refused to listen on port %1")
                   .arg(tunnel.bindPort));
        libssh2_session_disconnect(sess, "bye"); libssh2_session_free(sess); platform::net::closeSocket(sshFd);
        return;
    }
    libssh2_session_set_blocking(sess, 0);

    struct Pair { LIBSSH2_CHANNEL* chan; int fd; };
    std::vector<Pair> pairs;
    char buf[16384];
    while (!stopping->load()) {
        // Accept a new forwarded channel, if any, and dial the local target.
        LIBSSH2_CHANNEL* c = libssh2_channel_forward_accept(listener);
        if (c) {
            const int tfd = dialTcp(tunnel.targetHost.toUtf8(), tunnel.targetPort);
            if (tfd >= 0) pairs.push_back({c, tfd});
            else libssh2_channel_free(c);
        }
        // Service each active pair (channel <-> local socket).
        for (size_t i = 0; i < pairs.size();) {
            bool dead = false;
            // channel -> local fd
            for (;;) {
                const ssize_t n = libssh2_channel_read(pairs[i].chan, buf, sizeof(buf));
                if (n == LIBSSH2_ERROR_EAGAIN) break;
                if (n <= 0) { dead = true; break; }
                ::send(pairs[i].fd, buf, n, 0);
            }
            // local fd -> channel
            if (!dead) {
                if (platform::net::pollReadable(pairs[i].fd, 0) > 0) {
                    const ssize_t n = ::recv(pairs[i].fd, buf, sizeof(buf), 0);
                    if (n <= 0) dead = true;
                    else {
                        int off = 0;
                        while (off < n) {
                            const ssize_t w = libssh2_channel_write(pairs[i].chan, buf + off, n - off);
                            if (w == LIBSSH2_ERROR_EAGAIN) continue;
                            if (w < 0) { dead = true; break; }
                            off += w;
                        }
                    }
                }
            }
            if (dead || libssh2_channel_eof(pairs[i].chan)) {
                libssh2_channel_free(pairs[i].chan);
                platform::net::closeSocket(pairs[i].fd);
                pairs.erase(pairs.begin() + i);
            } else {
                ++i;
            }
        }
        platform::net::pollReadable(sshFd, pairs.empty() ? 50 : 5);   // avoid busy-spin
    }
    for (auto& p : pairs) { libssh2_channel_free(p.chan); platform::net::closeSocket(p.fd); }
    libssh2_channel_forward_cancel(listener);
    libssh2_session_disconnect(sess, "bye");
    libssh2_session_free(sess);
    platform::net::closeSocket(sshFd);
}
} // namespace

bool SshTunnel::start(const core::Session& sshServer, const Tunnel& tunnel) {
    stop();
    m_sshServer = sshServer;
    m_tunnel = tunnel;
    m_stopping = false;

    if (tunnel.kind == TunnelKind::Remote) {
        m_remoteListener = std::make_shared<std::thread>(
            remoteListenerWorker, m_sshServer, m_tunnel, &m_stopping,
            [this](QString m) { emit error(m); });
        return true;
    }

    // Local + Dynamic both listen locally.
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &SshTunnel::onNewConnection);
    if (!m_server->listen(QHostAddress(tunnel.bindAddr), tunnel.bindPort)) {
        emit error(QStringLiteral("Tunnel: failed to listen on %1:%2")
                       .arg(tunnel.bindAddr).arg(tunnel.bindPort));
        return false;
    }
    return true;
}

void SshTunnel::stop() {
    m_stopping = true;
    if (m_server) { m_server->close(); m_server->deleteLater(); m_server = nullptr; }
    for (auto& t : m_workers) if (t && t->joinable()) t->join();
    m_workers.clear();
    if (m_remoteListener && m_remoteListener->joinable()) m_remoteListener->join();
    m_remoteListener.reset();
}

void SshTunnel::onNewConnection() {
    while (m_server && m_server->hasPendingConnections()) {
        QTcpSocket* client = m_server->nextPendingConnection();
        const auto fd = client->socketDescriptor();
        if (fd < 0) { client->deleteLater(); continue; }
        // Duplicate the descriptor so the worker owns a stable fd, then drop the
        // QTcpSocket wrapper (it must not keep reading on the UI thread). On Windows
        // dupSocket uses WSADuplicateSocket; on Unix it is dup().
        const int dupFd = platform::net::dupSocket(int(fd));
        client->close();
        client->deleteLater();
        if (dupFd < 0) continue;
        m_workers.push_back(std::make_shared<std::thread>(
            relayWorker, m_sshServer, m_tunnel, dupFd, &m_stopping));
    }
}

} // namespace macxterm::tunnel
