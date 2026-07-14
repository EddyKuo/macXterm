#pragma once
#include <QString>
#include <QKeySequence>
#include <QHash>

namespace macxterm::core {

// Editable keyboard-shortcut registry (research §1.2 shortcut editor). Holds a
// named action → key-sequence map with MobaXterm-like defaults; supports
// rebinding and detecting conflicts.
class ShortcutRegistry {
public:
    ShortcutRegistry() { loadDefaults(); }

    QKeySequence sequence(const QString& action) const { return m_map.value(action); }
    bool has(const QString& action) const { return m_map.contains(action); }
    QStringList actions() const { return m_map.keys(); }

    // Rebind an action. Returns false if `seq` already maps to another action
    // (conflict), leaving the registry unchanged.
    bool rebind(const QString& action, const QKeySequence& seq);

    // Which action (if any) currently owns `seq`.
    QString actionFor(const QKeySequence& seq) const;

    void loadDefaults();

private:
    QHash<QString, QKeySequence> m_map;
};

} // namespace macxterm::core
