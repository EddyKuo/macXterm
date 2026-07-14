#include "core/IniStore.h"
#include <QFile>
#include <QStringList>
#include <QMap>

namespace macxterm::core {

// ── Serialize ──────────────────────────────────────────────────────────
static void collect(const SessionFolder& f, const QString& path, QByteArray& out) {
    const QString header = path.isEmpty() ? f.name() : path;
    out += "[Folder=" + header.toUtf8() + "]\n";
    for (const Session& s : f.sessions()) {
        QStringList kv;
        kv << sessionTypeToString(s.type());
        // Deterministic key order for stable output / round-trip tests.
        QStringList keys = s.params().keys();
        keys.sort();
        for (const QString& k : keys) {
            kv << (k + "=" + s.param(k));
        }
        out += s.name().toUtf8() + "=" + kv.join(';').toUtf8() + "\n";
    }
    for (const auto& sub : f.folders()) {
        const QString subPath = header + "/" + sub->name();
        collect(*sub, subPath, out);
    }
}

QByteArray IniStore::serialize(const SessionFolder& root) {
    QByteArray out;
    collect(root, QString(), out);
    return out;
}

// ── Deserialize ────────────────────────────────────────────────────────
// Ensure a folder exists at the "/"-delimited path (relative to root's children).
static SessionFolder* ensurePath(SessionFolder& root, const QString& fullPath) {
    QStringList parts = fullPath.split('/', Qt::SkipEmptyParts);
    if (parts.isEmpty()) return &root;
    // First component equals the root's own name.
    if (parts.first() == root.name()) parts.removeFirst();
    SessionFolder* cur = &root;
    for (const QString& part : parts) {
        SessionFolder* next = nullptr;
        for (const auto& f : cur->folders()) {
            if (f->name() == part) { next = f.get(); break; }
        }
        cur = next ? next : cur->addFolder(part);
    }
    return cur;
}

SessionFolder IniStore::deserialize(const QByteArray& data) {
    SessionFolder root(QStringLiteral("Sessions"));
    SessionFolder* cur = &root;
    const QStringList lines = QString::fromUtf8(data).split('\n');
    for (QString line : lines) {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        if (line.startsWith("[Folder=") && line.endsWith(']')) {
            const QString path = line.mid(8, line.size() - 9);
            cur = ensurePath(root, path);
            continue;
        }
        const int eq = line.indexOf('=');
        if (eq <= 0) continue;
        const QString name = line.left(eq);
        const QStringList fields = line.mid(eq + 1).split(';');
        if (fields.isEmpty()) continue;
        Session s(name, sessionTypeFromString(fields.first()));
        for (int i = 1; i < fields.size(); ++i) {
            const int e = fields[i].indexOf('=');
            if (e > 0) s.setParam(fields[i].left(e), fields[i].mid(e + 1));
        }
        cur->addSession(s);
    }
    return root;
}

bool IniStore::save(const SessionFolder& root, const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    return f.write(serialize(root)) != -1;
}

bool IniStore::load(SessionFolder& out, const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    out = deserialize(f.readAll());
    return true;
}

} // namespace macxterm::core
