#pragma once
#include <QObject>
#include <QString>

class QTcpServer;

namespace macxterm::tools {

// Minimal built-in HTTP file server (research §1.1 light daemons). Serves files
// from a root directory over GET; no MobaXterm-Home runtime cap (PRD G3).
// Intentionally tiny — enough for quick file sharing, not a general web server.
class HttpServer : public QObject {
    Q_OBJECT
public:
    explicit HttpServer(QObject* parent = nullptr);
    ~HttpServer() override;

    // Serve `rootDir` on bind:port (0 = ephemeral). Returns false on listen fail.
    bool start(const QString& rootDir, quint16 port = 0,
               const QString& bindAddr = QStringLiteral("127.0.0.1"));
    void stop();
    quint16 port() const;
    bool isRunning() const;

    // Pure request-line parser, exposed for tests: extracts the path from
    // "GET /path HTTP/1.1". Returns empty on malformed/unsupported requests.
    static QString parseRequestPath(const QByteArray& requestLine);

private slots:
    void onNewConnection();

private:
    QTcpServer* m_server = nullptr;
    QString m_root;
};

} // namespace macxterm::tools
