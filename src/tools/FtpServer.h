#pragma once
#include <QObject>
#include <QString>
#include <QHash>

class QTcpServer;
class QTcpSocket;

namespace macxterm::tools {

// Built-in FTP server (RFC 959 subset) over TCP, wrapping the tested FtpCommand
// codec. Anonymous access to a served root directory, with passive-mode data
// transfers: USER/PASS/SYST/PWD/CWD/TYPE/PASV/LIST/RETR/STOR/QUIT. Enough for
// quick file serving and to exercise the FtpConnection client end-to-end.
class FtpServer : public QObject {
    Q_OBJECT
public:
    explicit FtpServer(QObject* parent = nullptr);
    ~FtpServer() override;

    bool start(quint16 port = 0, const QString& bindAddr = QStringLiteral("127.0.0.1"));
    void stop();
    quint16 port() const;
    bool isRunning() const;

    // Directory served to clients (defaults to the process working directory).
    void setRootDir(const QString& dir) { m_root = dir; }
    QString rootDir() const { return m_root; }

private slots:
    void onNewConnection();

private:
    // Per-control-connection state: current dir (relative to root) and the
    // passive data-channel listener opened by PASV.
    struct Conn {
        QString cwd = QStringLiteral("/");
        QTcpServer* dataSrv = nullptr;
        QString storeTarget;    // absolute local path for an in-progress STOR
    };
    void handleLine(QTcpSocket* c, const QByteArray& line);
    QString resolve(const Conn& conn, const QString& arg) const;  // → absolute local path
    QTcpSocket* acceptData(Conn& conn);                            // accept the PASV data socket

    QTcpServer* m_server = nullptr;
    QString m_root;
    QString m_bindAddr = QStringLiteral("127.0.0.1");
    QHash<QTcpSocket*, Conn> m_conns;
};

} // namespace macxterm::tools
