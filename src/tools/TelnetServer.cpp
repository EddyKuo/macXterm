#include "tools/TelnetServer.h"
#include "platform/Pty.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QByteArray>
#include <cstdlib>

namespace macxterm::tools {

TelnetServer::TelnetServer(QObject* parent) : QObject(parent) {}
TelnetServer::~TelnetServer() { stop(); }

bool TelnetServer::start(quint16 port) {
    stop();
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &TelnetServer::onNewConnection);
    if (!m_server->listen(QHostAddress::LocalHost, port)) {
        m_server->deleteLater(); m_server = nullptr;
        return false;
    }
    return true;
}

void TelnetServer::stop() {
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        if (it.value()) { it.value()->terminate(); it.value()->deleteLater(); }
        if (it.key()) it.key()->deleteLater();
    }
    m_sessions.clear();
    if (m_server) { m_server->close(); m_server->deleteLater(); m_server = nullptr; }
}

bool TelnetServer::isRunning() const { return m_server && m_server->isListening(); }
quint16 TelnetServer::port() const { return m_server ? m_server->serverPort() : 0; }

void TelnetServer::onNewConnection() {
    while (m_server && m_server->hasPendingConnections()) {
        QTcpSocket* sock = m_server->nextPendingConnection();
        auto* pty = new platform::Pty(this);
        const char* shell = std::getenv("SHELL");
        if (!pty->start(shell && *shell ? QString::fromLocal8Bit(shell)
                                        : QStringLiteral("/bin/sh"))) {
            pty->deleteLater();
            sock->disconnectFromHost();
            continue;
        }
        m_sessions.insert(sock, pty);

        // PTY output → client.
        connect(pty, &platform::Pty::readyRead, sock, [sock](const QByteArray& b) {
            if (sock->state() == QAbstractSocket::ConnectedState) sock->write(b);
        });
        // Client input → PTY.
        connect(sock, &QTcpSocket::readyRead, pty, [sock, pty] {
            pty->write(sock->readAll());
        });
        auto cleanup = [this, sock] {
            if (auto* p = m_sessions.value(sock)) { p->terminate(); p->deleteLater(); }
            m_sessions.remove(sock);
            sock->deleteLater();
        };
        connect(sock, &QTcpSocket::disconnected, this, cleanup);
        connect(pty, &platform::Pty::finished, this, [sock] { sock->disconnectFromHost(); });
    }
}

} // namespace macxterm::tools
