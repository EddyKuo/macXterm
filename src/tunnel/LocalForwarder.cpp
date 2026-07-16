#include "tunnel/LocalForwarder.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <memory>

namespace macxterm::tunnel {

LocalForwarder::LocalForwarder(QObject* parent) : QObject(parent) {}
LocalForwarder::~LocalForwarder() { stop(); }

bool LocalForwarder::start(const QString& bindAddr, quint16 bindPort,
                           const QString& targetHost, quint16 targetPort) {
    stop();
    m_targetHost = targetHost;
    m_targetPort = targetPort;
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &LocalForwarder::onNewConnection);
    return m_server->listen(QHostAddress(bindAddr), bindPort);
}

void LocalForwarder::stop() {
    if (m_server) { m_server->deleteLater(); m_server = nullptr; }
    m_active = 0;
}

quint16 LocalForwarder::listenPort() const {
    return m_server ? m_server->serverPort() : 0;
}

bool LocalForwarder::isListening() const {
    return m_server && m_server->isListening();
}

void LocalForwarder::onNewConnection() {
    while (m_server && m_server->hasPendingConnections()) {
        QTcpSocket* client = m_server->nextPendingConnection();
        auto* upstream = new QTcpSocket(this);
        upstream->connectToHost(m_targetHost, m_targetPort);

        // Bidirectional pipe. QAbstractSocket queues writes issued before the
        // connection completes and flushes them on connect, so an unconditional
        // write() is safe even while `upstream` is still connecting.
        auto pump = [](QTcpSocket* from, QTcpSocket* to) {
            to->write(from->readAll());
        };
        connect(client, &QTcpSocket::readyRead, upstream, [pump, client, upstream] { pump(client, upstream); });
        connect(upstream, &QTcpSocket::readyRead, client, [pump, upstream, client] { pump(upstream, client); });
        // Data may already be buffered on the just-accepted socket before we
        // connected readyRead — drain it so the first bytes aren't lost.
        if (client->bytesAvailable() > 0) pump(client, upstream);

        // Both sockets typically disconnect in quick succession; a run-once
        // guard keeps teardown (and the --m_active it performs) to exactly one
        // execution per logical connection, so connectionCount() stays accurate.
        auto done = std::make_shared<bool>(false);
        auto teardown = [this, client, upstream, done] {
            if (*done) return;
            *done = true;
            client->deleteLater();
            upstream->deleteLater();
            if (m_active > 0) { --m_active; emit connectionCount(m_active); }
        };
        connect(client, &QTcpSocket::disconnected, this, teardown);
        connect(upstream, &QTcpSocket::disconnected, this, teardown);

        ++m_active;
        emit connectionCount(m_active);
    }
}

} // namespace macxterm::tunnel
