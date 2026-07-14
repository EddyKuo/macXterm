#include "tunnel/Tunnel.h"

namespace macxterm::tunnel {

QString tunnelKindToString(TunnelKind k) {
    switch (k) {
        case TunnelKind::Local:   return QStringLiteral("local");
        case TunnelKind::Remote:  return QStringLiteral("remote");
        case TunnelKind::Dynamic: return QStringLiteral("dynamic");
    }
    return QStringLiteral("local");
}

bool TunnelManager::bindPortInUse(quint16 port) const {
    for (const Tunnel& t : m_tunnels) if (t.bindPort == port) return true;
    return false;
}

bool TunnelManager::add(const Tunnel& t) {
    if (!t.isValid()) return false;
    if (bindPortInUse(t.bindPort)) return false;
    m_tunnels.push_back(t);
    return true;
}

} // namespace macxterm::tunnel
