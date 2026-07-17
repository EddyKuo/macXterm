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

static Session* find(QList<Session>& list, const QString& name) {
    for (Session& s : list) if (s.name() == name) return &s;
    return nullptr;
}

bool renameSessionInList(QList<Session>& list, const QString& oldName, const QString& newName) {
    const QString target = newName.trimmed();
    if (target.isEmpty()) return false;
    Session* self = find(list, oldName);
    if (!self) return false;
    if (target == oldName) return true;   // no-op rename
    // Reject a collision with any *other* session.
    for (const Session& s : list)
        if (&s != self && s.name() == target) return false;
    self->setName(target);
    return true;
}

bool moveSessionToFolder(QList<Session>& list, const QString& name, const QString& folder) {
    Session* s = find(list, name);
    if (!s) return false;
    s->setParam(QStringLiteral("folder"), folder.trimmed());
    return true;
}

bool setSessionIcon(QList<Session>& list, const QString& name, const QString& icon) {
    Session* s = find(list, name);
    if (!s) return false;
    s->setParam(QStringLiteral("icon"), icon.trimmed());
    return true;
}

int renameFolderInList(QList<Session>& list, const QString& oldFolder, const QString& newFolder) {
    const QString to = newFolder.trimmed();
    int changed = 0;
    for (Session& s : list) {
        if (s.param(QStringLiteral("folder")) == oldFolder) {
            s.setParam(QStringLiteral("folder"), to);
            ++changed;
        }
    }
    return changed;
}

QString uniqueCopyName(const QList<Session>& list, const QString& base) {
    const auto taken = [&](const QString& n) {
        for (const Session& s : list) if (s.name() == n) return true;
        return false;
    };
    QString candidate = base + QStringLiteral(" (copy)");
    for (int n = 2; taken(candidate); ++n)
        candidate = base + QStringLiteral(" (copy %1)").arg(n);
    return candidate;
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
