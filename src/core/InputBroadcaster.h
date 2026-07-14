#pragma once
#include <QByteArray>
#include <QHash>
#include <functional>

namespace macxterm::core {

// MultiExec logic (research §1.2), decoupled from widgets for testability.
// Terminals register as targets with a write-callback; broadcast() sends the
// same bytes to every ENABLED target and reports how many received. A target
// can opt out (MobaXterm's per-pane "disable from MultiExec"), and dead/closed
// targets are skipped by disabling them.
class InputBroadcaster {
public:
    using Sink = std::function<void(const QByteArray&)>;

    // Register a target; returns its id. Enabled by default.
    int addTarget(Sink sink) {
        const int id = m_next++;
        m_targets.insert(id, Target{std::move(sink), true});
        return id;
    }
    void removeTarget(int id) { m_targets.remove(id); }
    void setEnabled(int id, bool on) { if (m_targets.contains(id)) m_targets[id].enabled = on; }
    bool isEnabled(int id) const { return m_targets.contains(id) && m_targets[id].enabled; }
    int targetCount() const { return m_targets.size(); }
    int enabledCount() const {
        int n = 0;
        for (const auto& t : m_targets) if (t.enabled) ++n;
        return n;
    }

    // Send bytes to every enabled target; returns the number that received.
    int broadcast(const QByteArray& data) {
        int n = 0;
        for (auto& t : m_targets) {
            if (t.enabled && t.sink) { t.sink(data); ++n; }
        }
        return n;
    }

private:
    struct Target { Sink sink; bool enabled; };
    QHash<int, Target> m_targets;
    int m_next = 1;
};

} // namespace macxterm::core
