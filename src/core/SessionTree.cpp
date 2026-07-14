#include "core/SessionTree.h"

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

} // namespace macxterm::core
