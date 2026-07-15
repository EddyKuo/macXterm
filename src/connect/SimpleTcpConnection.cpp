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
    if (type == SessionType::Rsh) {
        // RFC-1282-style rcmd handshake: stderr-port(ASCII)\0 local-user\0
        // remote-user\0 command\0. We use a single channel (stderr-port "0"),
        // fold stderr into the main stream, and take the command from the
        // "remotecommand" param (empty runs the remote default login shell).
        const QByteArray remote = session.username().toUtf8();
        const QByteArray local  = session.param("localuser", session.username()).toUtf8();
        const QByteArray cmd    = session.param("remotecommand").toUtf8();
        QByteArray h;
        h.append('0'); h.append('\0');       // no separate stderr port
        h.append(local);  h.append('\0');
        h.append(remote); h.append('\0');
        h.append(cmd);    h.append('\0');
        return h;
    }
    if (type == SessionType::Xdmcp) {
        // XDMCP Query opcode marker (bootstrap); full handshake is UDP in prod.
        return QByteArray("\0\1\0\2", 4);
    }
    return {};
}

bool SimpleTcpConnection::expectsAckByte(SessionType type) {
    return type == SessionType::Rlogin || type == SessionType::Rsh;
}

bool SimpleTcpConnection::connectSession(const core::Session& session) {
    setState(State::Connecting);
    m_sock = new QTcpSocket(this);
    connect(m_sock, &QTcpSocket::readyRead, this, &SimpleTcpConnection::onReadyRead);
    m_awaitingAck = expectsAckByte(m_type);
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
    QByteArray data = m_sock->readAll();
    // Rlogin/Rsh open with a single 0x00 status byte (0 = success). Swallow it
    // so it never reaches the terminal; a non-zero status flags a login error.
    if (m_awaitingAck && !data.isEmpty()) {
        const char status = data.at(0);
        data.remove(0, 1);
        m_awaitingAck = false;
        if (status != 0) {
            setState(State::Failed);
            emit errorOccurred(QStringLiteral("Remote rejected the connection"));
            return;
        }
    }
    if (!data.isEmpty()) emit dataReceived(data);
}

qint64 SimpleTcpConnection::send(const QByteArray& data) {
    return m_sock ? m_sock->write(data) : -1;
}

void SimpleTcpConnection::disconnectSession() {
    if (m_sock) { m_sock->disconnectFromHost(); m_sock->deleteLater(); m_sock = nullptr; }
    setState(State::Disconnected);
}

} // namespace macxterm::connect
