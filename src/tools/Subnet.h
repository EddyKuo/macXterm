#pragma once
#include <QString>
#include <QStringList>

namespace macxterm::tools {

// IPv4 subnet helpers for the network scanner. Expands CIDR notation
// ("192.168.1.0/24") into the list of host addresses to probe. Pure and
// unit-tested. To keep scans bounded, prefixes shorter than /16 are rejected
// (returns empty).
class Subnet {
public:
    // Expand a CIDR block into host IPs. For /31 and /32 all addresses are
    // returned; otherwise the network and broadcast addresses are excluded.
    // Returns an empty list on malformed input or an over-large range.
    static QStringList hosts(const QString& cidr);

    // Parse "a.b.c.d" into a 32-bit host-order integer. Returns false on error.
    static bool parseIpv4(const QString& s, quint32& out);
    static QString formatIpv4(quint32 v);
};

} // namespace macxterm::tools
