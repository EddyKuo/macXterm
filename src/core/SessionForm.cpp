#include "core/SessionForm.h"
#include <QStringList>

namespace macxterm::core {

static const QStringList kCoreFields = {"name", "type", "host", "port", "username"};

Session SessionForm::toSession(const QVariantMap& f) {
    Session s(f.value("name").toString(),
              sessionTypeFromString(f.value("type").toString()));
    if (f.contains("host"))     s.setHost(f.value("host").toString());
    if (f.contains("username")) s.setUsername(f.value("username").toString());
    if (f.contains("port")) {
        bool ok = false;
        const int p = f.value("port").toInt(&ok);
        if (ok && p > 0) s.setPort(p);
    }
    // Pass through any extra protocol params.
    for (auto it = f.constBegin(); it != f.constEnd(); ++it) {
        if (!kCoreFields.contains(it.key())) s.setParam(it.key(), it.value().toString());
    }
    return s;
}

QVariantMap SessionForm::fromSession(const Session& s) {
    QVariantMap f;
    f.insert("name", s.name());
    f.insert("type", sessionTypeToString(s.type()));
    f.insert("host", s.host());
    f.insert("username", s.username());
    // Only serialize an explicitly-set port, not the protocol default that
    // Session::port() would synthesize — otherwise a round-trip invents one.
    if (s.params().contains("port")) f.insert("port", s.param("port"));
    const QVariantMap& p = s.params();
    for (auto it = p.constBegin(); it != p.constEnd(); ++it) {
        if (!kCoreFields.contains(it.key())) f.insert(it.key(), it.value());
    }
    return f;
}

QString SessionForm::validate(const QVariantMap& f) {
    if (f.value("name").toString().trimmed().isEmpty())
        return QStringLiteral("Session name is required");
    const SessionType t = sessionTypeFromString(f.value("type").toString());
    if (t == SessionType::Unknown)
        return QStringLiteral("Unknown session type");
    // Network protocols need a host; local Shell does not.
    const bool needsHost = (t != SessionType::Shell && t != SessionType::Serial);
    if (needsHost && f.value("host").toString().trimmed().isEmpty())
        return QStringLiteral("Host is required for this session type");
    if (t == SessionType::Serial && f.value("port").toString().trimmed().isEmpty())
        return QStringLiteral("Serial port is required");
    return {};
}

} // namespace macxterm::core
