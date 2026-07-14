#include "tunnel/SshTunnel.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <libssh2.h>

#if !defined(_WIN32)
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <poll.h>
#endif

namespace macxterm::tunnel {

SshTunnel::SshTunnel(QObject* parent) : QObject(parent) { libssh2_init(0); }
SshTunnel::~SshTunnel() { stop(); libssh2_exit(); }

bool SshTunnel::isRunning() const { return m_server && m_server->isListening(); }
quint16 SshTunnel::listenPort() const { return m_server ? m_server->serverPort() : 0; }

bool SshTunnel::start(const core::Session& sshServer, const Tunnel& tunnel) {
    stop();
    if (tunnel.kind != TunnelKind::Local) {
        emit error(QStringLiteral("Only local tunnels are supported for now"));
        return false;
    }
    m_sshServer = sshServer;
    m_tunnel = tunnel;
    m_stopping = false;
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
}

#if !defined(_WIN32)
namespace {
// Open a raw TCP socket to host:port (blocking). Returns fd or -1.
int dialTcp(const QByteArray& host, int port) {
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host.constData(), QByteArray::number(port).constData(), &hints, &res) != 0)
        return -1;
    int fd = -1;
    for (auto* p = res; p; p = p->ai_next) {
        fd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;
        if (::connect(fd, p->ai_addr, p->ai_addrlen) == 0) break;
        ::close(fd); fd = -1;
    }
    freeaddrinfo(res);
    return fd;
}

// Blocking relay between a client fd and an SSH direct-tcpip channel: for each
// incoming client connection open its own SSH session (avoids sharing one
// non-thread-safe session across workers), then pump bytes both directions.
void relayWorker(core::Session sshServer, Tunnel tunnel, int clientFd,
                 const std::atomic<bool>* stopping) {
    const int sshFd = dialTcp(sshServer.host().toUtf8(), sshServer.port());
    if (sshFd < 0) { ::close(clientFd); return; }
    LIBSSH2_SESSION* sess = libssh2_session_init();
    if (!sess) { ::close(sshFd); ::close(clientFd); return; }
    libssh2_session_set_blocking(sess, 1);
    bool ok = libssh2_session_handshake(sess, sshFd) == 0;
    if (ok) {
        const QByteArray user = sshServer.username().toUtf8();
        const QByteArray key = sshServer.param("keyfile").toUtf8();
        if (!key.isEmpty()) {
            const QByteArray pass = sshServer.param("passphrase").toUtf8();
            ok = libssh2_userauth_publickey_fromfile(sess, user.constData(), nullptr,
                                                     key.constData(), pass.constData()) == 0;
        } else {
            const QByteArray pw = sshServer.param("password").toUtf8();
            ok = libssh2_userauth_password(sess, user.constData(), pw.constData()) == 0;
        }
    }
    LIBSSH2_CHANNEL* chan = ok
        ? libssh2_channel_direct_tcpip_ex(sess, tunnel.targetHost.toUtf8().constData(),
                                          tunnel.targetPort, "127.0.0.1", 0)
        : nullptr;
    if (chan) {
        libssh2_session_set_blocking(sess, 0);
        char buf[16384];
        while (!stopping->load()) {
            struct pollfd pfd{clientFd, POLLIN, 0};
            const int pr = ::poll(&pfd, 1, 20);
            if (pr > 0 && (pfd.revents & POLLIN)) {
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
            // Drain channel -> client.
            for (;;) {
                const ssize_t n = libssh2_channel_read(chan, buf, sizeof(buf));
                if (n == LIBSSH2_ERROR_EAGAIN) break;
                if (n <= 0) break;
                ::send(clientFd, buf, n, 0);
            }
            if (libssh2_channel_eof(chan)) break;
        }
        libssh2_channel_free(chan);
    }
    if (sess) { libssh2_session_disconnect(sess, "bye"); libssh2_session_free(sess); }
    ::close(sshFd);
    ::close(clientFd);
}
} // namespace

void SshTunnel::onNewConnection() {
    while (m_server && m_server->hasPendingConnections()) {
        QTcpSocket* client = m_server->nextPendingConnection();
        const auto fd = client->socketDescriptor();
        if (fd < 0) { client->deleteLater(); continue; }
        // Duplicate the descriptor so the worker owns a stable fd, then drop the
        // QTcpSocket wrapper (it must not keep reading on the UI thread).
        const int dupFd = ::dup(int(fd));
        client->close();
        client->deleteLater();
        if (dupFd < 0) continue;
        m_workers.push_back(std::make_shared<std::thread>(
            relayWorker, m_sshServer, m_tunnel, dupFd, &m_stopping));
    }
}
#else
void SshTunnel::onNewConnection() {
    emit error(QStringLiteral("SSH tunnels on Windows not yet wired"));
}
#endif

} // namespace macxterm::tunnel
