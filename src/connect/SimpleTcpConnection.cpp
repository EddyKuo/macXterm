#include "connect/SimpleTcpConnection.h"
#include <QTcpSocket>

namespace macxterm::connect {
using core::SessionType;

SimpleTcpConnection::SimpleTcpConnection(SessionType type, QObject* parent)
    : IConnection(parent), m_type(type) {}

QByteArray SimpleTcpConnection::startupHandshake(SessionType type, const core::Session& session) {
    if (type == SessionType::Rlogin) {
        // RFC 1282: \0 client-user \0 server-user \0 terminal/speed \0
        const QByteArray user = session.username().toUtf8();
        QByteArray h;
        h.append('\0');
        h.append(user); h.append('\0');
        h.append(user); h.append('\0');
        h.append("xterm-256color/38400"); h.append('\0');
        return h;
    }
    if (type == SessionType::Xdmcp) {
        // XDMCP Query opcode marker (bootstrap); full handshake is UDP in prod.
        return QByteArray("\0\1\0\2", 4);
    }
    // RSH: no in-band handshake in this simplified client.
    return {};
}

bool SimpleTcpConnection::connectSession(const core::Session& session) {
    setState(State::Connecting);
    m_sock = new QTcpSocket(this);
    connect(m_sock, &QTcpSocket::readyRead, this, &SimpleTcpConnection::onReadyRead);
    connect(m_sock, &QTcpSocket::connected, this, [this, session] {
        const QByteArray h = startupHandshake(m_type, session);
        if (!h.isEmpty()) m_sock->write(h);
        setState(State::Connected);
    });
    connect(m_sock, &QTcpSocket::disconnected, this, [this] { setState(State::Closed); });
    connect(m_sock, &QTcpSocket::errorOccurred, this, [this] {
        setState(State::Failed);
        emit errorOccurred(m_sock->errorString());
    });
    m_sock->connectToHost(session.host(), static_cast<quint16>(session.port()));
    return true;
}

void SimpleTcpConnection::onReadyRead() {
    emit dataReceived(m_sock->readAll());
}

qint64 SimpleTcpConnection::send(const QByteArray& data) {
    return m_sock ? m_sock->write(data) : -1;
}

void SimpleTcpConnection::disconnectSession() {
    if (m_sock) { m_sock->disconnectFromHost(); m_sock->deleteLater(); m_sock = nullptr; }
    setState(State::Disconnected);
}

} // namespace macxterm::connect
