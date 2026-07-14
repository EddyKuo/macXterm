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

} // namespace macxterm::core
