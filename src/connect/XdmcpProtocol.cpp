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

} // namespace macxterm::connect::xdmcp
