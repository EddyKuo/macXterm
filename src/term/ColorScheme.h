#pragma once
#include <QString>
#include <QColor>
#include <array>

namespace macxterm::term {

// Terminal color scheme: 16 ANSI colors + default fg/bg (research §1.2 themes).
// Provides a few built-in schemes and hex parsing for user customization.
class ColorScheme {
public:
    ColorScheme();   // defaults to the Dark scheme (defined in .cpp, no recursion)

    QColor ansi(int index) const { return m_ansi[index & 0x0f]; }
    void setAnsi(int index, const QColor& c) { m_ansi[index & 0x0f] = c; }
    QColor foreground() const { return m_fg; }
    QColor background() const { return m_bg; }
    void setForeground(const QColor& c) { m_fg = c; }
    void setBackground(const QColor& c) { m_bg = c; }
    const QString& name() const { return m_name; }

    // Built-in schemes.
    static ColorScheme dark();
    static ColorScheme light();
    static ColorScheme solarizedDark();
    static ColorScheme byName(const QString& name);   // falls back to dark()

private:
    QString m_name;
    std::array<QColor, 16> m_ansi;
    QColor m_fg;
    QColor m_bg;
};

} // namespace macxterm::term
