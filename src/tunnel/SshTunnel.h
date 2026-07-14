#pragma once
#include "tunnel/Tunnel.h"
#include "core/Session.h"
#include <QObject>
#include <atomic>
#include <thread>
#include <vector>
#include <memory>

class QTcpServer;

namespace macxterm::tunnel {

// A real SSH tunnel supporting all three MobaXterm kinds:
//   - Local (-L):   listen on bind:bindPort, forward each connection through an
//                   SSH direct-tcpip channel to target:targetPort.
//   - Dynamic (-D): listen on bind:bindPort as a SOCKS4/4a/5 proxy; the target
//                   is negotiated per-connection and forwarded via direct-tcpip.
//   - Remote (-R):  ask the SSH server to listen on bind:bindPort and forward
//                   accepted channels back to a locally-dialed target:targetPort.
// Local/Dynamic run a QTcpServer with one worker thread per accepted client
// (each opens its own libssh2 session — safe). Remote runs a single listener
// worker that multiplexes all forwarded channels on one session (libssh2
// sessions are not thread-safe). The SSH server + credentials come from a Session.
class SshTunnel : public QObject {
    Q_OBJECT
public:
    explicit SshTunnel(QObject* parent = nullptr);
    ~SshTunnel() override;

    // Open the SSH gateway session and start the tunnel. Supports Local, Dynamic
    // and Remote kinds. Returns false on listen/auth failure.
    bool start(const core::Session& sshServer, const Tunnel& tunnel);
    void stop();
    bool isRunning() const;
    quint16 listenPort() const;

signals:
    void error(const QString& message);

private slots:
    void onNewConnection();

private:
    QTcpServer* m_server = nullptr;
    core::Session m_sshServer;
    Tunnel m_tunnel;
    std::atomic<bool> m_stopping{false};
    std::vector<std::shared_ptr<std::thread>> m_workers;
    std::shared_ptr<std::thread> m_remoteListener;   // Remote (-R) accept loop
};

} // namespace macxterm::tunnel
