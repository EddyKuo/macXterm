#include "core/ShortcutRegistry.h"

namespace macxterm::core {

void ShortcutRegistry::loadDefaults() {
    m_map.clear();
    // QKeySequence resolves Qt::CTRL to ⌘ on macOS and Ctrl elsewhere — one
    // definition, cross-platform behavior (UI_Spec theming notes).
    m_map.insert("terminal.new",       QKeySequence("Ctrl+Alt+T"));
    m_map.insert("terminal.close",     QKeySequence("Ctrl+Shift+W"));
    m_map.insert("tab.next",           QKeySequence("Ctrl+Tab"));
    m_map.insert("tab.prev",           QKeySequence("Ctrl+Shift+Tab"));
    m_map.insert("tab.detach",         QKeySequence("Ctrl+Shift+D"));
    m_map.insert("view.fullscreen",    QKeySequence("F11"));
    m_map.insert("edit.paste",         QKeySequence("Shift+Ins"));
    m_map.insert("session.quickConnect", QKeySequence("Ctrl+Shift+Q"));
    m_map.insert("x11.toggle",         QKeySequence("Ctrl+Shift+X"));
    m_map.insert("editor.open",        QKeySequence("Ctrl+Shift+M"));
}

QString ShortcutRegistry::actionFor(const QKeySequence& seq) const {
    for (auto it = m_map.constBegin(); it != m_map.constEnd(); ++it) {
        if (it.value() == seq) return it.key();
    }
    return {};
}

bool ShortcutRegistry::rebind(const QString& action, const QKeySequence& seq) {
    const QString owner = actionFor(seq);
    if (!owner.isEmpty() && owner != action) return false;   // conflict
    m_map.insert(action, seq);
    return true;
}

} // namespace macxterm::core
