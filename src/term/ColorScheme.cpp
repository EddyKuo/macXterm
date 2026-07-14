#include "term/ColorScheme.h"

namespace macxterm::term {

static std::array<QColor, 16> xtermBase() {
    return {
        QColor(0,0,0),       QColor(205,0,0),    QColor(0,205,0),    QColor(205,205,0),
        QColor(0,0,238),     QColor(205,0,205),  QColor(0,205,205),  QColor(229,229,229),
        QColor(127,127,127), QColor(255,0,0),    QColor(0,255,0),    QColor(255,255,0),
        QColor(92,92,255),   QColor(255,0,255),  QColor(0,255,255),  QColor(255,255,255),
    };
}

// Default constructor populates the Dark scheme directly (no call to dark(),
// which would recurse back into this constructor and overflow the stack).
ColorScheme::ColorScheme() {
    m_name = QStringLiteral("Dark");
    m_ansi = xtermBase();
    m_fg = QColor(229, 229, 229);
    m_bg = QColor(0, 0, 0);
}

ColorScheme ColorScheme::dark() {
    return ColorScheme();   // default IS the dark scheme
}

ColorScheme ColorScheme::light() {
    ColorScheme s;
    s.m_name = QStringLiteral("Light");
    s.m_ansi = xtermBase();
    s.m_fg = QColor(0, 0, 0);
    s.m_bg = QColor(255, 255, 255);
    return s;
}

ColorScheme ColorScheme::solarizedDark() {
    ColorScheme s;
    s.m_name = QStringLiteral("Solarized Dark");
    s.m_ansi = xtermBase();
    s.m_ansi[0]  = QColor(0x07, 0x36, 0x42);
    s.m_ansi[15] = QColor(0xfd, 0xf6, 0xe3);
    s.m_fg = QColor(0x83, 0x94, 0x96);
    s.m_bg = QColor(0x00, 0x2b, 0x36);
    return s;
}

ColorScheme ColorScheme::byName(const QString& name) {
    const QString n = name.toLower();
    if (n == "light") return light();
    if (n == "solarized dark" || n == "solarized") return solarizedDark();
    return dark();
}

} // namespace macxterm::term
