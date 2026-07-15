#include "connect/XdmcpConnection.h"
#include "connect/XdmcpProtocol.h"
#include <QUdpSocket>
#include <QHostInfo>

namespace macxterm::connect {
using namespace xdmcp;

XdmcpConnection::XdmcpConnection(QObject* parent) : QObject(parent) {}
XdmcpConnection::~XdmcpConnection() { stop(); }

// The client's IPv4 address as 4 network-order bytes, for the Request's
// connection-address field. Falls back to loopback when the bound socket has no
// specific address (e.g. bound to AnyIPv4).
static QByteArray ipv4Bytes(const QHostAddress& addr) {
    const quint32 v4 = addr.toIPv4Address();
    const quint32 use = v4 ? v4 : QHostAddress(QHostAddress::LocalHost).toIPv4Address();
    QByteArray b;
    b.append(char((use >> 24) & 0xff)); b.append(char((use >> 16) & 0xff));
    b.append(char((use >> 8) & 0xff));  b.append(char(use & 0xff));
    return b;
}

bool XdmcpConnection::start(const QString& host, quint16 port) {
    // Resolve the peer: accept a literal address directly, else look it up.
    QHostAddress peer(host);
    if (peer.isNull()) {
        const QHostInfo info = QHostInfo::fromName(host);
        if (info.addresses().isEmpty()) { emit failed(QStringLiteral("Cannot resolve host")); return false; }
        peer = info.addresses().first();
    }
    m_peer = peer;
    m_peerPort = port;

    m_sock = new QUdpSocket(this);
    if (!m_sock->bind(QHostAddress::AnyIPv4, 0)) {
        emit failed(QStringLiteral("Cannot bind UDP socket"));
        m_sock->deleteLater(); m_sock = nullptr;
        return false;
    }
    connect(m_sock, &QUdpSocket::readyRead, this, &XdmcpConnection::onReadyRead);
    m_phase = Phase::Query;
    m_sock->writeDatagram(encodeQuery({}), m_peer, m_peerPort);
    return true;
}

void XdmcpConnection::stop() {
    if (m_sock) { m_sock->close(); m_sock->deleteLater(); m_sock = nullptr; }
    m_phase = Phase::Idle;
}

void XdmcpConnection::onReadyRead() {
    while (m_sock && m_sock->hasPendingDatagrams()) {
        QByteArray dg(m_sock->pendingDatagramSize(), '\0');
        m_sock->readDatagram(dg.data(), dg.size());
        const Header h = parseHeader(dg);
        if (!h.valid) continue;

        if (isRejection(h.opcode)) { m_phase = Phase::Done; emit rejected(h.opcode); return; }

        if (m_phase == Phase::Query && h.opcode == Willing) {
            const WillingInfo w = parseWilling(dg);
            if (!w.valid) continue;
            emit willing(w.hostname, w.status);
            // Answer with a Request: display 0, Internet/IPv4 connection type +
            // our address, no authentication, MIT-MAGIC-COOKIE-1 authorization.
            RequestParams rp;
            rp.connectionTypes = {0};
            rp.connectionAddresses = { ipv4Bytes(m_sock->localAddress()) };
            rp.authorizationNames = { QByteArray("MIT-MAGIC-COOKIE-1") };
            m_sock->writeDatagram(encodeRequest(rp), m_peer, m_peerPort);
            m_phase = Phase::Request;
        } else if (m_phase == Phase::Request && h.opcode == Accept) {
            const AcceptInfo a = parseAccept(dg);
            if (!a.valid) continue;
            m_phase = Phase::Done;
            emit accepted(a.sessionId);
        }
    }
}

} // namespace macxterm::connect
