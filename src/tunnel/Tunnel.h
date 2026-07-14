#pragma once
#include <QString>
#include <QList>

namespace macxterm::tunnel {

// SSH tunnel specification (Architecture §4 tunnel/; research §1.6).
// local:   listen on bind:bindPort locally, forward to target:targetPort via SSH
// remote:  listen on the SSH server, forward back to target:targetPort locally
// dynamic: SOCKS proxy on bind:bindPort (no fixed target)
enum class TunnelKind { Local, Remote, Dynamic };

QString tunnelKindToString(TunnelKind k);

struct Tunnel {
    TunnelKind kind = TunnelKind::Local;
    QString bindAddr = QStringLiteral("127.0.0.1");
    quint16 bindPort = 0;
    QString targetHost;      // unused for Dynamic
    quint16 targetPort = 0;  // unused for Dynamic
    bool enabled = true;

    // A tunnel is valid if it has a bind port and (for L/R) a target.
    bool isValid() const {
        if (bindPort == 0) return false;
        if (kind == TunnelKind::Dynamic) return true;
        return !targetHost.isEmpty() && targetPort != 0;
    }
};

// Owns a set of tunnels for a session; rejects invalid or port-colliding ones.
class TunnelManager {
public:
    // Returns false (and does not add) if invalid or the bind port is taken.
    bool add(const Tunnel& t);
    void removeAt(int i) { if (i >= 0 && i < m_tunnels.size()) m_tunnels.removeAt(i); }
    const QList<Tunnel>& tunnels() const { return m_tunnels; }
    int count() const { return m_tunnels.size(); }
    bool bindPortInUse(quint16 port) const;

private:
    QList<Tunnel> m_tunnels;
};

} // namespace macxterm::tunnel
