#pragma once
#include <QObject>
#include <QByteArray>
#include <QHostAddress>

class QUdpSocket;

namespace macxterm::connect {

// XDMCP discovery + session-negotiation driver over UDP (port 177): sends a
// Query, and on a Willing reply sends a Request, resolving to an Accept (session
// id) or a rejection (Decline/Refuse/Failed). Launching a local X server to
// actually display the remote desktop after Accept needs a real display-manager
// peer and is out of scope here — this class drives and (via a loopback fixture)
// verifies the handshake state machine.
class XdmcpConnection : public QObject {
    Q_OBJECT
public:
    explicit XdmcpConnection(QObject* parent = nullptr);
    ~XdmcpConnection() override;

    // Begin the handshake against host:port. Returns false if the UDP socket
    // can't bind or the host can't be resolved.
    bool start(const QString& host, quint16 port = 177);
    void stop();

signals:
    void willing(const QByteArray& hostname, const QByteArray& status);
    void accepted(quint32 sessionId);
    void rejected(quint16 opcode);       // Decline / Refuse / Failed
    void failed(const QString& error);

private slots:
    void onReadyRead();

private:
    enum class Phase { Idle, Query, Request, Done };
    QUdpSocket* m_sock = nullptr;
    QHostAddress m_peer;
    quint16 m_peerPort = 177;
    Phase m_phase = Phase::Idle;
};

} // namespace macxterm::connect
