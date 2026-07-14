#pragma once
#include <QObject>
#include <QString>
#include <QHash>
#include <QByteArray>

class QUdpSocket;

namespace macxterm::tools {

// Built-in read-only TFTP server (RFC 1350) over UDP, wrapping the tested
// TftpPacket codec. Serves files from a root directory; no runtime cap
// (PRD G3). Handles RRQ + block/ACK; WRQ is refused with an ERROR packet.
class TftpServer : public QObject {
    Q_OBJECT
public:
    explicit TftpServer(QObject* parent = nullptr);
    ~TftpServer() override;

    bool start(const QString& rootDir, quint16 port = 0,
               const QString& bindAddr = QStringLiteral("127.0.0.1"));
    void stop();
    quint16 port() const;
    bool isRunning() const;

private slots:
    void onDatagram();

private:
    struct Transfer { QByteArray data; int nextBlock = 1; };

    QUdpSocket* m_sock = nullptr;
    QString m_root;
    QHash<QString, Transfer> m_transfers;   // keyed by "host:port"
};

} // namespace macxterm::tools
