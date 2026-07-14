#pragma once
#include <QString>
#include <QVariant>
#include <QVariantMap>

namespace macxterm::core {

// Global application settings (Terminal/X11/Display/General groups, mirroring
// MobaXterm's settings tabs — research/MobaXterm.md §11). Thin typed wrapper
// over a key/value map; persistence handled by the caller.
class Settings {
public:
    // Defaults matching a sensible terminal baseline.
    Settings();

    QVariant value(const QString& key, const QVariant& def = {}) const {
        return m_values.value(key, def);
    }
    void setValue(const QString& key, const QVariant& v) { m_values.insert(key, v); }

    // Common typed accessors.
    QString fontFamily() const { return value("terminal.font", "Menlo").toString(); }
    int fontSize() const { return value("terminal.fontSize", 12).toInt(); }
    QString colorScheme() const { return value("terminal.scheme", "Dark").toString(); }
    bool x11AutoStart() const { return value("x11.autoStart", true).toBool(); }
    int scrollbackLines() const { return value("terminal.scrollback", 10000).toInt(); }

    const QVariantMap& all() const { return m_values; }

private:
    QVariantMap m_values;
};

} // namespace macxterm::core
