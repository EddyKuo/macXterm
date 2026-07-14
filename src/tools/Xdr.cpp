#include "tools/Xdr.h"

namespace macxterm::tools {

void XdrWriter::u32(quint32 v) {
    char b[4] = {char(v >> 24), char(v >> 16), char(v >> 8), char(v)};
    m_buf.append(b, 4);
}

void XdrWriter::u64(quint64 v) {
    u32(static_cast<quint32>(v >> 32));
    u32(static_cast<quint32>(v));
}

void XdrWriter::opaqueFixed(const QByteArray& b) {
    m_buf.append(b);
    while (m_buf.size() % 4 != 0) m_buf.append('\0');
}

void XdrWriter::opaqueVar(const QByteArray& b) {
    u32(static_cast<quint32>(b.size()));
    opaqueFixed(b);
}

quint32 XdrReader::u32(bool* ok) {
    if (m_pos + 4 > m_buf.size()) { if (ok) *ok = false; return 0; }
    const auto* p = reinterpret_cast<const unsigned char*>(m_buf.constData() + m_pos);
    const quint32 v = (quint32(p[0]) << 24) | (quint32(p[1]) << 16) |
                      (quint32(p[2]) << 8) | quint32(p[3]);
    m_pos += 4;
    if (ok) *ok = true;
    return v;
}

quint64 XdrReader::u64(bool* ok) {
    bool a = false, b = false;
    const quint64 hi = u32(&a);
    const quint64 lo = u32(&b);
    if (ok) *ok = a && b;
    return (hi << 32) | lo;
}

QByteArray XdrReader::opaqueFixed(int n, bool* ok) {
    const int padded = (n + 3) & ~3;
    if (m_pos + padded > m_buf.size()) { if (ok) *ok = false; return {}; }
    const QByteArray out = m_buf.mid(m_pos, n);
    m_pos += padded;
    if (ok) *ok = true;
    return out;
}

QByteArray XdrReader::opaqueVar(bool* ok) {
    bool a = false;
    const quint32 n = u32(&a);
    if (!a || n > quint32(m_buf.size())) { if (ok) *ok = false; return {}; }
    return opaqueFixed(static_cast<int>(n), ok);
}

} // namespace macxterm::tools
