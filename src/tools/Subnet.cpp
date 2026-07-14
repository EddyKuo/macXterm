#include "tools/Subnet.h"

namespace macxterm::tools {

bool Subnet::parseIpv4(const QString& s, quint32& out) {
    const QStringList parts = s.split('.');
    if (parts.size() != 4) return false;
    quint32 v = 0;
    for (const QString& p : parts) {
        bool ok = false;
        const uint b = p.toUInt(&ok);
        if (!ok || b > 255 || p.isEmpty()) return false;
        v = (v << 8) | b;
    }
    out = v;
    return true;
}

QString Subnet::formatIpv4(quint32 v) {
    return QStringLiteral("%1.%2.%3.%4")
        .arg((v >> 24) & 0xff).arg((v >> 16) & 0xff).arg((v >> 8) & 0xff).arg(v & 0xff);
}

QStringList Subnet::hosts(const QString& cidr) {
    const int slash = cidr.indexOf('/');
    if (slash < 0) return {};
    quint32 base = 0;
    if (!parseIpv4(cidr.left(slash), base)) return {};
    bool ok = false;
    const int prefix = cidr.mid(slash + 1).toInt(&ok);
    if (!ok || prefix < 16 || prefix > 32) return {};   // bound the range at /16

    const quint32 mask = (prefix == 0) ? 0u : (0xffffffffu << (32 - prefix));
    const quint32 network = base & mask;
    const quint32 broadcast = network | ~mask;

    QStringList out;
    if (prefix >= 31) {
        for (quint32 v = network; ; ++v) { out << formatIpv4(v); if (v == broadcast) break; }
    } else {
        // Exclude network + broadcast addresses.
        for (quint32 v = network + 1; v < broadcast; ++v) out << formatIpv4(v);
    }
    return out;
}

} // namespace macxterm::tools
