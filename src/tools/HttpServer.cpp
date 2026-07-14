#include "tools/HttpServer.h"
#include "sftp/RemotePath.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace macxterm::tools {

HttpServer::HttpServer(QObject* parent) : QObject(parent) {}
HttpServer::~HttpServer() { stop(); }

QString HttpServer::parseRequestPath(const QByteArray& requestLine) {
    const QList<QByteArray> parts = requestLine.trimmed().split(' ');
    if (parts.size() < 2 || parts[0] != "GET") return {};
    QString path = QString::fromUtf8(parts[1]);
    const int q = path.indexOf('?');
    if (q >= 0) path = path.left(q);
    return path;
}

bool HttpServer::start(const QString& rootDir, quint16 port, const QString& bindAddr) {
    stop();
    m_root = QFileInfo(rootDir).absoluteFilePath();
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &HttpServer::onNewConnection);
    return m_server->listen(QHostAddress(bindAddr), port);
}

void HttpServer::stop() {
    if (m_server) { m_server->deleteLater(); m_server = nullptr; }
}

quint16 HttpServer::port() const { return m_server ? m_server->serverPort() : 0; }
bool HttpServer::isRunning() const { return m_server && m_server->isListening(); }

void HttpServer::onNewConnection() {
    while (m_server && m_server->hasPendingConnections()) {
        QTcpSocket* c = m_server->nextPendingConnection();
        auto handle = [this, c] {
            if (!c->canReadLine()) return;
            const QByteArray req = c->readLine();
            QString path = parseRequestPath(req);
            auto respond = [c](int code, const QString& status, const QByteArray& body,
                               const QByteArray& type = "application/octet-stream") {
                QByteArray h = "HTTP/1.1 " + QByteArray::number(code) + " " + status.toUtf8() + "\r\n";
                h += "Content-Type: " + type + "\r\n";
                h += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
                h += "Connection: close\r\n\r\n";
                c->write(h);
                c->write(body);
                c->disconnectFromHost();
            };
            if (path.isEmpty()) { respond(400, "Bad Request", "bad request"); return; }
            // Confine to root: normalize and reject traversal outside m_root.
            const QString rel = sftp::RemotePath::normalize(path);
            const QString full = QFileInfo(m_root + "/" + rel).absoluteFilePath();
            if (!full.startsWith(m_root) || !QFileInfo(full).isFile()) {
                respond(404, "Not Found", "not found", "text/plain");
                return;
            }
            QFile f(full);
            if (!f.open(QIODevice::ReadOnly)) { respond(403, "Forbidden", "forbidden", "text/plain"); return; }
            respond(200, "OK", f.readAll());
        };
        connect(c, &QTcpSocket::readyRead, c, handle);
        // The request line may already be buffered before readyRead was wired.
        if (c->canReadLine()) handle();
    }
}

} // namespace macxterm::tools
