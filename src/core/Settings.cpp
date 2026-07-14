#include "core/Settings.h"

namespace macxterm::core {

Settings::Settings() {
    m_values.insert("terminal.font", "Menlo");
    m_values.insert("terminal.fontSize", 12);
    m_values.insert("terminal.scheme", "Dark");
    m_values.insert("terminal.scrollback", 10000);
    m_values.insert("terminal.encoding", "UTF-8");
    m_values.insert("x11.autoStart", true);
    m_values.insert("x11.displayMode", "multiwindow");
    m_values.insert("ssh.x11Forwarding", true);
    m_values.insert("ssh.compression", true);
}

} // namespace macxterm::core
