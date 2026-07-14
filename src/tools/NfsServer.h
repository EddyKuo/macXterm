#pragma once
#include <QObject>
#include <QString>
#include <QHash>
#include <QByteArray>

class QUdpSocket;

namespace macxterm::tools {

// A minimal read-focused NFSv3 server (MobaXterm's embedded NFS server), built
// directly on ONC-RPC over UDP — no rpcbind/OS NFS dependency. It answers the
// portmapper (GETPORT), the MOUNT v3 protocol (MNT/UMNT/EXPORT) and the NFS v3
// procedures a client needs to mount and browse/read an exported directory
// (GETATTR/LOOKUP/ACCESS/READ/READDIR/FSINFO/FSSTAT/PATHCONF). Writes are not
// served. Bind to localhost or a trusted network.
//
// Note: real clients often expect the portmapper on port 111, which needs
// privilege; start() binds NFS/MOUNT on `port` and the portmapper on 111 only
// if permitted (else clients must be pointed at `port` directly).
class NfsServer : public QObject {
    Q_OBJECT
public:
    explicit NfsServer(QObject* parent = nullptr);

    bool start(const QString& exportDir, quint16 port = 2049);
    void stop();
    bool isRunning() const;
    quint16 port() const { return m_port; }

    // Exposed for unit testing: build an RPC reply for one request datagram.
    // Returns an empty array if the datagram is not a well-formed RPC call.
    QByteArray handleDatagram(const QByteArray& request);

private slots:
    void onReadyRead();

private:
    QByteArray fileHandle(const QString& path);   // assign/lookup a 32-byte fh
    QString pathForHandle(const QByteArray& fh) const { return m_handleToPath.value(fh); }

    QUdpSocket* m_nfs = nullptr;
    QUdpSocket* m_portmap = nullptr;
    QString m_export;
    quint16 m_port = 0;
    QHash<QByteArray, QString> m_handleToPath;
    QHash<QString, QByteArray> m_pathToHandle;
    quint32 m_nextHandle = 1;
};

} // namespace macxterm::tools
