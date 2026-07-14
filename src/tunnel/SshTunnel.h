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

// A real SSH local tunnel: listens on bind:bindPort and forwards each accepted
// connection through an SSH direct-tcpip channel to target:targetPort. Each
// accepted client is handled on its own worker thread doing a blocking relay
// between the client socket and a libssh2 channel (keeps libssh2's blocking I/O
// off the Qt event loop). The SSH server + credentials come from a Session.
class SshTunnel : public QObject {
    Q_OBJECT
public:
    explicit SshTunnel(QObject* parent = nullptr);
    ~SshTunnel() override;

    // Open the SSH gateway session and start listening. Currently supports Local
    // tunnels. Returns false on listen/auth failure.
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
};

} // namespace macxterm::tunnel
