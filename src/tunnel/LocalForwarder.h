#pragma once
#include <QObject>
#include <QString>

class QTcpServer;

namespace macxterm::tunnel {

// The data path for a local SSH tunnel (Architecture §6.3). Listens on a local
// bind port; for each accepted connection it opens a socket to target:port and
// relays bytes both ways. In production the target leg is an SSH direct-tcpip
// channel; here it is a plain TCP socket so the forwarding logic is testable
// end-to-end over loopback (swap the target transport, keep the plumbing).
class LocalForwarder : public QObject {
    Q_OBJECT
public:
    explicit LocalForwarder(QObject* parent = nullptr);
    ~LocalForwarder() override;

    // Start listening on bindAddr:bindPort, relaying to targetHost:targetPort.
    // Returns false if the listen fails (e.g. port in use). Pass bindPort 0 to
    // get an ephemeral port (read it back via listenPort()).
    bool start(const QString& bindAddr, quint16 bindPort,
               const QString& targetHost, quint16 targetPort);
    void stop();
    quint16 listenPort() const;
    bool isListening() const;

signals:
    void connectionCount(int active);

private slots:
    void onNewConnection();

private:
    QTcpServer* m_server = nullptr;
    QString m_targetHost;
    quint16 m_targetPort = 0;
    int m_active = 0;
};

} // namespace macxterm::tunnel
