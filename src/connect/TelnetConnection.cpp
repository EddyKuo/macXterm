#include "connect/TelnetConnection.h"
#include <QTcpSocket>

namespace macxterm::connect {

TelnetConnection::TelnetConnection(QObject* parent) : IConnection(parent) {}

bool TelnetConnection::connectSession(const core::Session& session) {
    setState(State::Connecting);
    m_sock = new QTcpSocket(this);
    connect(m_sock, &QTcpSocket::readyRead, this, &TelnetConnection::onReadyRead);
    connect(m_sock, &QTcpSocket::connected, this, [this] { setState(State::Connected); });
    connect(m_sock, &QTcpSocket::disconnected, this, [this] { setState(State::Closed); });
    connect(m_sock, &QTcpSocket::errorOccurred, this, [this] {
        setState(State::Failed);
        emit errorOccurred(m_sock->errorString());
    });
    m_sock->connectToHost(session.host(), static_cast<quint16>(session.port()));
    return true;
}

void TelnetConnection::onReadyRead() {
    const QByteArray in = m_sock->readAll();
    const TelnetProtocol::Result r = m_proto.process(in);
    if (!r.response.isEmpty()) m_sock->write(r.response);
    if (!r.appData.isEmpty()) emit dataReceived(r.appData);
}

qint64 TelnetConnection::send(const QByteArray& data) {
    if (!m_sock) return -1;
    return m_sock->write(data);
}

void TelnetConnection::disconnectSession() {
    if (m_sock) { m_sock->disconnectFromHost(); m_sock->deleteLater(); m_sock = nullptr; }
    setState(State::Disconnected);
}

} // namespace macxterm::connect
