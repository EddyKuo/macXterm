#pragma once
#include <QObject>
#include <QString>

class QTcpServer;
class QTcpSocket;

namespace macxterm::tools {

// Built-in FTP control server (RFC 959 subset) over TCP, wrapping the tested
// FtpCommand codec. Anonymous read access; enough for quick file serving
// (research §1.1). Data-channel transfers are added in a later phase; this
// layer handles the control dialog (USER/PASS/SYST/PWD/TYPE/QUIT).
class FtpServer : public QObject {
    Q_OBJECT
public:
    explicit FtpServer(QObject* parent = nullptr);
    ~FtpServer() override;

    bool start(quint16 port = 0, const QString& bindAddr = QStringLiteral("127.0.0.1"));
    void stop();
    quint16 port() const;
    bool isRunning() const;

private slots:
    void onNewConnection();

private:
    void handleLine(QTcpSocket* c, const QByteArray& line);

    QTcpServer* m_server = nullptr;
};

} // namespace macxterm::tools
