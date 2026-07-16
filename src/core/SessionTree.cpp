#include "core/SessionTree.h"
#include <QStringList>

namespace macxterm::core {

SessionFolder* SessionFolder::addFolder(const QString& name) {
    auto f = std::make_shared<SessionFolder>(name);
    m_folders.push_back(f);
    return f.get();
}

int SessionFolder::totalSessions() const {
    int n = m_sessions.size();
    for (const auto& f : m_folders) n += f->totalSessions();
    return n;
}

const Session* SessionFolder::findSession(const QString& name) const {
    for (const auto& s : m_sessions) {
        if (s.name() == name) return &s;
    }
    for (const auto& f : m_folders) {
        if (const Session* hit = f->findSession(name)) return hit;
    }
    return nullptr;
}

QList<FolderGroup> groupSessionsByFolder(const QList<Session>& sessions) {
    QList<FolderGroup> groups;
    // Loose sessions always occupy the first (empty-label) group.
    groups.append(FolderGroup{QString(), {}});
    auto indexOf = [&](const QString& folder) -> int {
        for (int i = 0; i < groups.size(); ++i)
            if (groups[i].folder == folder) return i;
        groups.append(FolderGroup{folder, {}});
        return groups.size() - 1;
    };
    for (const Session& s : sessions)
        groups[indexOf(s.param(QStringLiteral("folder")))].sessions.push_back(s);
    // Drop the loose group if nothing landed in it, so an all-foldered list
    // doesn't render a stray empty root bucket.
    if (groups.first().sessions.isEmpty()) groups.removeFirst();
    return groups;
}

QString sessionGlyph(const Session& s) {
    const QString icon = s.param(QStringLiteral("icon"));
    if (!icon.isEmpty()) return icon;
    switch (s.type()) {
        case SessionType::Ssh:    return QStringLiteral("🔑");
        case SessionType::Telnet: return QStringLiteral("📟");
        case SessionType::Rsh:
        case SessionType::Rlogin: return QStringLiteral("📡");
        case SessionType::Serial: return QStringLiteral("🔌");
        case SessionType::Mosh:   return QStringLiteral("📶");
        case SessionType::Sftp:
        case SessionType::Ftp:    return QStringLiteral("📁");
        case SessionType::S3:     return QStringLiteral("🪣");
        case SessionType::Shell:  return QStringLiteral("🐚");
        case SessionType::Rdp:    return QStringLiteral("🖥️");
        case SessionType::Vnc:    return QStringLiteral("👁️");
        case SessionType::Xdmcp:  return QStringLiteral("🪟");
        case SessionType::Browser:return QStringLiteral("🌐");
        default:                  return QStringLiteral("•");
    }
}

bool sessionMatchesFilter(const Session& s, const QString& query) {
    const QString q = query.trimmed();
    if (q.isEmpty()) return true;
    const auto has = [&](const QString& field) {
        return field.contains(q, Qt::CaseInsensitive);
    };
    return has(s.name()) || has(s.host()) || has(s.username())
        || has(s.param(QStringLiteral("folder")));
}

QStringList folderNames(const QList<Session>& sessions) {
    QStringList names;
    for (const Session& s : sessions) {
        const QString f = s.param(QStringLiteral("folder"));
        if (!f.isEmpty() && !names.contains(f)) names.append(f);
    }
    return names;
}

} // namespace macxterm::core
