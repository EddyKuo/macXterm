#include "connect/XdmcpProtocol.h"

namespace macxterm::connect::xdmcp {

// Append a big-endian CARD16.
static void putU16(QByteArray& b, quint16 v) {
    b.append(char((v >> 8) & 0xff));
    b.append(char(v & 0xff));
}

static quint16 getU16(const QByteArray& b, int off) {
    return (static_cast<quint8>(b[off]) << 8) | static_cast<quint8>(b[off + 1]);
}

// Append an ARRAY8: CARD8 length then the bytes.
static void putArray8(QByteArray& b, const QByteArray& a) {
    b.append(char(a.size() & 0xff));
    b.append(a);
}

Header parseHeader(const QByteArray& buf) {
    Header h;
    if (buf.size() < 6) return h;
    h.version = getU16(buf, 0);
    h.opcode  = getU16(buf, 2);
    h.length  = getU16(buf, 4);
    h.valid = true;
    return h;
}

QByteArray encodeQuery(const QList<QByteArray>& authNames) {
    QByteArray body;
    body.append(char(authNames.size() & 0xff));   // ARRAYofARRAY8 count
    for (const QByteArray& n : authNames) putArray8(body, n);

    QByteArray pkt;
    putU16(pkt, kVersion);
    putU16(pkt, Query);
    putU16(pkt, static_cast<quint16>(body.size()));
    pkt.append(body);
    return pkt;
}

WillingInfo parseWilling(const QByteArray& buf) {
    WillingInfo w;
    const Header h = parseHeader(buf);
    if (!h.valid || h.opcode != Willing) return w;
    // Read three consecutive ARRAY8 fields from the body.
    int off = 6;
    auto readArray8 = [&](QByteArray& out) -> bool {
        if (off + 1 > buf.size()) return false;
        const int len = static_cast<quint8>(buf[off++]);
        if (off + len > buf.size()) return false;
        out = buf.mid(off, len);
        off += len;
        return true;
    };
    if (!readArray8(w.authenticationName)) return w;
    if (!readArray8(w.hostname)) return w;
    if (!readArray8(w.status)) return w;
    w.valid = true;
    return w;
}

QByteArray encodeRequest(const RequestParams& p) {
    QByteArray body;
    putU16(body, p.displayNumber);
    // ARRAY16 connection-types: CARD8 count + count×CARD16.
    body.append(char(p.connectionTypes.size() & 0xff));
    for (quint16 t : p.connectionTypes) putU16(body, t);
    // ARRAYofARRAY8 connection-addresses.
    body.append(char(p.connectionAddresses.size() & 0xff));
    for (const QByteArray& a : p.connectionAddresses) putArray8(body, a);
    putArray8(body, p.authenticationName);
    putArray8(body, p.authenticationData);
    // ARRAYofARRAY8 authorization-names.
    body.append(char(p.authorizationNames.size() & 0xff));
    for (const QByteArray& a : p.authorizationNames) putArray8(body, a);
    putArray8(body, p.manufacturerDisplayID);

    QByteArray pkt;
    putU16(pkt, kVersion);
    putU16(pkt, Request);
    putU16(pkt, static_cast<quint16>(body.size()));
    pkt.append(body);
    return pkt;
}

AcceptInfo parseAccept(const QByteArray& buf) {
    AcceptInfo a;
    const Header h = parseHeader(buf);
    if (!h.valid || h.opcode != Accept) return a;
    int off = 6;
    if (off + 4 > buf.size()) return a;
    a.sessionId = (static_cast<quint32>(static_cast<quint8>(buf[off])) << 24)
                | (static_cast<quint32>(static_cast<quint8>(buf[off + 1])) << 16)
                | (static_cast<quint32>(static_cast<quint8>(buf[off + 2])) << 8)
                |  static_cast<quint32>(static_cast<quint8>(buf[off + 3]));
    off += 4;
    auto readArray8 = [&](QByteArray& out) -> bool {
        if (off + 1 > buf.size()) return false;
        const int len = static_cast<quint8>(buf[off++]);
        if (off + len > buf.size()) return false;
        out = buf.mid(off, len);
        off += len;
        return true;
    };
    if (!readArray8(a.authenticationName)) return a;
    if (!readArray8(a.authenticationData)) return a;
    if (!readArray8(a.authorizationName)) return a;
    if (!readArray8(a.authorizationData)) return a;
    a.valid = true;
    return a;
}

bool isRejection(quint16 opcode) {
    return opcode == Decline || opcode == Refuse || opcode == Failed;
}

} // namespace macxterm::connect::xdmcp
