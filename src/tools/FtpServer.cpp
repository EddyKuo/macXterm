#include "tools/FtpServer.h"
#include "tools/FtpCommand.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>

namespace macxterm::tools {

FtpServer::FtpServer(QObject* parent) : QObject(parent) {}
FtpServer::~FtpServer() { stop(); }

bool FtpServer::start(quint16 port, const QString& bindAddr) {
    stop();
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &FtpServer::onNewConnection);
    return m_server->listen(QHostAddress(bindAddr), port);
}

void FtpServer::stop() {
    if (m_server) { m_server->deleteLater(); m_server = nullptr; }
}

quint16 FtpServer::port() const { return m_server ? m_server->serverPort() : 0; }
bool FtpServer::isRunning() const { return m_server && m_server->isListening(); }

void FtpServer::onNewConnection() {
    while (m_server && m_server->hasPendingConnections()) {
        QTcpSocket* c = m_server->nextPendingConnection();
        c->write(ftp::reply(220, "macXterm FTP ready"));
        auto drain = [this, c] {
            while (c->canReadLine()) handleLine(c, c->readLine());
        };
        connect(c, &QTcpSocket::readyRead, c, drain);
        if (c->canReadLine()) drain();
    }
}

void FtpServer::handleLine(QTcpSocket* c, const QByteArray& line) {
    const ftp::Command cmd = ftp::parse(line);
    if (!cmd.valid) { c->write(ftp::reply(500, "Syntax error")); return; }

    const QString& v = cmd.verb;
    if (v == "USER")      c->write(ftp::reply(331, "User name okay, need password"));
    else if (v == "PASS") c->write(ftp::reply(230, "User logged in"));
    else if (v == "SYST") c->write(ftp::reply(215, "UNIX Type: L8"));
    else if (v == "PWD")  c->write(ftp::reply(257, "\"/\" is current directory"));
    else if (v == "TYPE") c->write(ftp::reply(200, "Type set to " + cmd.arg));
    else if (v == "NOOP") c->write(ftp::reply(200, "OK"));
    else if (v == "QUIT") { c->write(ftp::reply(221, "Goodbye")); c->disconnectFromHost(); }
    else                  c->write(ftp::reply(502, "Command not implemented"));
}

} // namespace macxterm::tools
