#include "connect/VncConnection.h"
#include <QTcpSocket>
#include <algorithm>

namespace macxterm::connect {

VncConnection::VncConnection(QObject* parent) : IConnection(parent) {}

bool VncConnection::connectSession(const core::Session& session) {
    setState(State::Connecting);
    m_phase = Phase::Version;
    m_buf.clear();
    m_sock = new QTcpSocket(this);
    connect(m_sock, &QTcpSocket::readyRead, this, &VncConnection::onReadyRead);
    connect(m_sock, &QTcpSocket::disconnected, this, [this] { setState(State::Closed); });
    connect(m_sock, &QTcpSocket::errorOccurred, this, [this] {
        setState(State::Failed);
        emit errorOccurred(m_sock->errorString());
    });
    m_sock->connectToHost(session.host(), static_cast<quint16>(session.port()));
    return true;
}

static quint16 rd16(const QByteArray& b, int off) {
    return (static_cast<quint8>(b[off]) << 8) | static_cast<quint8>(b[off + 1]);
}
static quint32 rd32(const QByteArray& b, int off) {
    return (static_cast<quint32>(static_cast<quint8>(b[off])) << 24)
         | (static_cast<quint32>(static_cast<quint8>(b[off + 1])) << 16)
         | (static_cast<quint32>(static_cast<quint8>(b[off + 2])) << 8)
         |  static_cast<quint32>(static_cast<quint8>(b[off + 3]));
}

void VncConnection::onReadyRead() {
    m_buf += m_sock->readAll();

    // Version: 12-byte ProtocolVersion → reply with the same, advance.
    if (m_phase == Phase::Version) {
        if (m_buf.size() < 12) return;
        const rfb::Version v = rfb::parseVersion(m_buf.left(12));
        if (!v.valid) { setState(State::Failed); emit errorOccurred("Bad RFB version"); return; }
        m_buf.remove(0, 12);
        m_sock->write(rfb::formatVersion(v));
        m_phase = Phase::Security;
    }

    // Security (3.8): count(1) + types; select "None" (1). Then SecurityResult.
    if (m_phase == Phase::Security) {
        if (m_buf.isEmpty()) return;
        const int n = static_cast<quint8>(m_buf[0]);
        if (n == 0) { setState(State::Failed); emit errorOccurred("Server rejected connection"); return; }
        if (m_buf.size() < 1 + n) return;
        const QByteArray types = m_buf.mid(1, n);
        m_buf.remove(0, 1 + n);
        char chosen = types.contains(char(1)) ? char(1) : types[0];
        m_sock->write(QByteArray(1, chosen));
        m_phase = Phase::SecurityResult;
    }

    if (m_phase == Phase::SecurityResult) {
        if (m_buf.size() < 4) return;
        const quint32 result = rd32(m_buf, 0);
        m_buf.remove(0, 4);
        if (result != 0) { setState(State::Failed); emit errorOccurred("VNC authentication failed"); return; }
        m_sock->write(QByteArray(1, char(1)));   // ClientInit: shared
        m_phase = Phase::ServerInit;
    }

    if (m_phase == Phase::ServerInit) {
        if (m_buf.size() < 24) return;
        const quint32 nameLen = rd32(m_buf, 20);
        if (m_buf.size() < int(24 + nameLen)) return;
        m_serverInit = rfb::parseServerInit(m_buf.left(24 + nameLen));
        m_buf.remove(0, 24 + nameLen);
        setState(State::Connected);
        emit serverReady(m_serverInit.width, m_serverInit.height, m_serverInit.name);
        // Advertise the encodings we can decode (preference order); the server
        // then compresses rectangles instead of only sending RAW.
        m_sock->write(rfb::encodeSetEncodings(
            {rfb::EncHextile, rfb::EncRRE, rfb::EncCopyRect, rfb::EncRaw}));
        requestFramebuffer();
        m_phase = Phase::Running;
    }

    if (m_phase == Phase::Running) {
        // Decode any complete FramebufferUpdate messages. Each rectangle's
        // payload length depends on its encoding, so decodeRect reports how many
        // bytes it consumed (or complete=false when we need to wait for more).
        while (!m_buf.isEmpty() && static_cast<quint8>(m_buf[0]) == 0) {
            const rfb::FramebufferUpdate hdr = rfb::parseFramebufferUpdate(m_buf);
            if (!hdr.valid) break;
            int off = 4;
            bool complete = true;
            for (const rfb::Rectangle& r : hdr.rects) {
                off += 12;   // step over this rect's header (already parsed)
                const rfb::RectData d = rfb::decodeRect(r, m_buf, off, 4);
                if (!d.complete) { complete = false; break; }
                if (d.isCopy)
                    emit copyRect(d.srcX, d.srcY, r.x, r.y, r.width, r.height);
                else
                    emit rectDecoded(r.x, r.y, r.width, r.height, d.pixels);
                off += d.consumed;
            }
            if (!complete) break;
            m_buf.remove(0, off);
        }
    }
}

void VncConnection::requestFramebuffer() {
    QByteArray req;
    req.append(char(3));                 // FramebufferUpdateRequest
    req.append(char(0));                 // incremental = 0 (full)
    auto p16 = [&](int v){ req.append(char((v >> 8) & 0xff)); req.append(char(v & 0xff)); };
    p16(0); p16(0);
    p16(m_serverInit.width); p16(m_serverInit.height);
    m_sock->write(req);
}

qint64 VncConnection::send(const QByteArray&) {
    return -1;   // VNC input events are structured; injected via the surface widget
}

void VncConnection::sendPointerEvent(int x, int y, int buttonMask) {
    if (m_viewOnly || m_phase != Phase::Running || !m_sock) return;
    m_buttonMask = buttonMask;
    m_sock->write(rfb::encodePointerEvent(x, y, buttonMask));
}

void VncConnection::sendKeyEvent(quint32 keysym, bool down) {
    if (m_viewOnly || m_phase != Phase::Running || !m_sock) return;
    m_sock->write(rfb::encodeKeyEvent(keysym, down));
}

void VncConnection::disconnectSession() {
    if (m_sock) { m_sock->disconnectFromHost(); m_sock->deleteLater(); m_sock = nullptr; }
    setState(State::Disconnected);
}

} // namespace macxterm::connect
