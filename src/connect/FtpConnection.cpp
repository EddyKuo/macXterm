#include "connect/FtpConnection.h"
#include <QTcpSocket>

namespace macxterm::connect {

FtpConnection::FtpConnection(QObject* parent) : IConnection(parent) {}

bool FtpConnection::connectSession(const core::Session& session) {
    setState(State::Connecting);
    m_username = session.username();
    m_sentUser = false;
    m_sock = new QTcpSocket(this);
    connect(m_sock, &QTcpSocket::readyRead, this, &FtpConnection::onReadyRead);
    connect(m_sock, &QTcpSocket::connected, this, [this] { setState(State::Connected); });
    connect(m_sock, &QTcpSocket::disconnected, this, [this] { setState(State::Closed); });
    connect(m_sock, &QTcpSocket::errorOccurred, this, [this] {
        setState(State::Failed);
        emit errorOccurred(m_sock->errorString());
    });
    const int port = session.port() > 0 ? session.port() : 21;
    m_sock->connectToHost(session.host(), static_cast<quint16>(port));
    return true;
}

void FtpConnection::onReadyRead() {
    const QByteArray data = m_sock->readAll();
    emit dataReceived(data);
    // Auto-send USER once, after the 220 greeting, for convenience.
    if (!m_sentUser && !m_username.isEmpty() && data.contains("220")) {
        m_sentUser = true;
        m_sock->write("USER " + m_username.toUtf8() + "\r\n");
    }
}

qint64 FtpConnection::send(const QByteArray& data) {
    return m_sock ? m_sock->write(data) : -1;
}

void FtpConnection::disconnectSession() {
    if (m_sock) { m_sock->disconnectFromHost(); m_sock->deleteLater(); m_sock = nullptr; }
    setState(State::Disconnected);
}

} // namespace macxterm::connect
