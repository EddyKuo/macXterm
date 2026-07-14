#pragma once
#include "tunnel/Tunnel.h"
#include <QVariantMap>
#include <QString>

namespace macxterm::tunnel {

// Pure mapping between a tunnel-editor form and a Tunnel, plus validation —
// keeps TunnelDialog thin and lets the logic be unit-tested (UI_Spec flow D).
class TunnelForm {
public:
    // Fields: "kind" (local/remote/dynamic), "bindAddr", "bindPort",
    // "targetHost", "targetPort".
    static Tunnel toTunnel(const QVariantMap& fields);
    static QVariantMap fromTunnel(const Tunnel& t);
    // Empty string if valid, else an error message.
    static QString validate(const QVariantMap& fields);
};

} // namespace macxterm::tunnel
