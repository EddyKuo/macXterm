#include "tunnel/TunnelForm.h"

namespace macxterm::tunnel {

static TunnelKind kindFromString(const QString& s) {
    const QString k = s.toLower();
    if (k == "remote") return TunnelKind::Remote;
    if (k == "dynamic") return TunnelKind::Dynamic;
    return TunnelKind::Local;
}

Tunnel TunnelForm::toTunnel(const QVariantMap& f) {
    Tunnel t;
    t.kind = kindFromString(f.value("kind").toString());
    if (f.contains("bindAddr")) t.bindAddr = f.value("bindAddr").toString();
    t.bindPort = static_cast<quint16>(f.value("bindPort").toUInt());
    t.targetHost = f.value("targetHost").toString();
    t.targetPort = static_cast<quint16>(f.value("targetPort").toUInt());
    return t;
}

QVariantMap TunnelForm::fromTunnel(const Tunnel& t) {
    QVariantMap f;
    f.insert("kind", tunnelKindToString(t.kind));
    f.insert("bindAddr", t.bindAddr);
    f.insert("bindPort", t.bindPort);
    f.insert("targetHost", t.targetHost);
    f.insert("targetPort", t.targetPort);
    return f;
}

QString TunnelForm::validate(const QVariantMap& f) {
    const Tunnel t = toTunnel(f);
    if (t.bindPort == 0) return QStringLiteral("Bind port is required");
    if (t.kind != TunnelKind::Dynamic) {
        if (t.targetHost.isEmpty()) return QStringLiteral("Target host is required");
        if (t.targetPort == 0) return QStringLiteral("Target port is required");
    }
    return {};
}

} // namespace macxterm::tunnel
