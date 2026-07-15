#pragma once
#include "core/Session.h"
#include <QString>
#include <QList>
#include <memory>

namespace macxterm::core {

// A folder in the session tree. Folders may contain sub-folders and sessions,
// mirroring MobaXterm's bookmark tree (research/MobaXterm.md §1.7).
class SessionFolder {
public:
    explicit SessionFolder(QString name = QStringLiteral("Sessions")) : m_name(std::move(name)) {}

    const QString& name() const { return m_name; }
    void setName(const QString& n) { m_name = n; }

    SessionFolder* addFolder(const QString& name);
    void addSession(const Session& s) { m_sessions.push_back(s); }

    const QList<std::shared_ptr<SessionFolder>>& folders() const { return m_folders; }
    QList<Session>& sessions() { return m_sessions; }
    const QList<Session>& sessions() const { return m_sessions; }

    // Total sessions in this subtree (recursive).
    int totalSessions() const;

    // Depth-first lookup by session name; returns nullptr if not found.
    const Session* findSession(const QString& name) const;

private:
    QString m_name;
    QList<std::shared_ptr<SessionFolder>> m_folders;
    QList<Session> m_sessions;
};

// One display group in the session tree: the folder label (empty = loose,
// top-level sessions) and the sessions filed under it.
struct FolderGroup {
    QString folder;
    QList<Session> sessions;
};

// Group a flat session list by each session's "folder" param for tree display.
// Loose sessions (no/blank folder) come first under an empty label, then each
// named folder in first-seen order; session order within a group is preserved.
// Pure — no GUI — so the grouping is unit-tested independent of the tree widget.
QList<FolderGroup> groupSessionsByFolder(const QList<Session>& sessions);

// The set of distinct folder names present in a session list, in first-seen
// order (used to populate the SessionDialog folder picker).
QStringList folderNames(const QList<Session>& sessions);

// A small emoji shown before a bookmark's name in the tree. An explicit "icon"
// param wins; otherwise it defaults from the session type. Pure/testable.
QString sessionGlyph(const Session& s);

} // namespace macxterm::core
