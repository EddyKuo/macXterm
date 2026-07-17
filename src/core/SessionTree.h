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

// ── Session-tree edit operations (pure; back the right-click context menu) ──
// These mutate a flat session list exactly the way MobaXterm's bookmark context
// menu does, encapsulating the collision / uniqueness / move rules that the UI
// would otherwise duplicate. Kept in core so the rules are unit-tested without
// the tree widget (see [[testable-core-architecture]]).

// Rename a session. Fails (returns false, list untouched) if newName is blank
// after trimming, if it collides with a *different* existing session, or if
// oldName is absent. Renaming to the same name is a successful no-op.
bool renameSessionInList(QList<Session>& list, const QString& oldName, const QString& newName);

// Set a session's "folder" param (blank = move to the loose top level). Returns
// false if the named session is absent.
bool moveSessionToFolder(QList<Session>& list, const QString& name, const QString& folder);

// Set a session's "icon" glyph (blank clears it, reverting to the type default).
// Returns false if the named session is absent.
bool setSessionIcon(QList<Session>& list, const QString& name, const QString& icon);

// Rename a folder: every session filed under oldFolder is moved to newFolder
// (blank newFolder empties them to the top level). Returns the number of
// sessions changed (0 if none matched oldFolder).
int renameFolderInList(QList<Session>& list, const QString& oldFolder, const QString& newFolder);

// A unique display name for a duplicated session: "<base> (copy)", then
// "<base> (copy 2)", "<base> (copy 3)", … skipping any name already present.
QString uniqueCopyName(const QList<Session>& list, const QString& base);

// Case-insensitive match of a session against a free-text filter query, testing
// the fields a user would search the bookmark tree by: name, host, username and
// folder. An empty/blank query matches everything. Pure — backs the tree's live
// filter box (MobaXterm parity) so the matching is unit-tested without the GUI.
bool sessionMatchesFilter(const Session& s, const QString& query);

} // namespace macxterm::core
