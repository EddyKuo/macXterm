#pragma once
#include <QChar>
#include <QString>
#include <QVector>

namespace macxterm::term {

// Append a full Unicode code point to a QString, emitting a UTF-16 surrogate
// pair when it lives outside the BMP. Use this wherever a Cell's `ch` becomes
// user-visible text (rendering, clipboard, toText).
inline void appendCodePoint(QString& s, char32_t cp) {
    if (QChar::requiresSurrogates(cp)) {
        s.append(QChar(QChar::highSurrogate(cp)));
        s.append(QChar(QChar::lowSurrogate(cp)));
    } else {
        s.append(QChar(static_cast<char16_t>(cp)));
    }
}

// One code point as a QString (for drawText / single-glyph rendering).
inline QString codePointString(char32_t cp) {
    QString s;
    appendCodePoint(s, cp);
    return s;
}

// Collapse a code point to a single UTF-16 unit for column-aligned buffers
// (syntax highlighting, scrollback search) where each cell must map to exactly
// one string index; astral glyphs fold to U+FFFD there.
inline QChar cellUnit(char32_t cp) {
    return cp <= 0xFFFF ? QChar(static_cast<char16_t>(cp))
                        : QChar(QChar::ReplacementCharacter);
}

// How a cell's foreground/background color is expressed. Default = use the
// active color scheme; Ansi = one of the 16 palette entries (scheme-controlled,
// so live-recolorable); Rgb = an absolute 24-bit color (256-color cube 16..255
// and true-color).
enum class CellColor : unsigned char { Default, Ansi, Rgb };

// A single terminal cell: one character plus SGR attributes and full color.
// `ch` holds a full Unicode code point (char32_t), so astral-plane glyphs
// (emoji, rare CJK) survive intact — one cell still means one printed glyph.
struct Cell {
    char32_t ch = U' ';
    CellColor fgKind = CellColor::Default;
    CellColor bgKind = CellColor::Default;
    unsigned char fgIndex = 7;   // valid when fgKind == Ansi
    unsigned char bgIndex = 0;   // valid when bgKind == Ansi
    unsigned int fgRgb = 0;      // 0xRRGGBB, valid when fgKind == Rgb
    unsigned int bgRgb = 0;
    bool bold = false;
    bool reverse = false;
    bool wide = false;           // double-width glyph (CJK): spans this + next cell
};

// A rows x cols grid of cells — the rendered terminal screen state.
class ScreenBuffer {
public:
    ScreenBuffer(int rows = 24, int cols = 80) { resize(rows, cols); }

    int rows() const { return m_rows; }
    int cols() const { return m_cols; }

    void resize(int rows, int cols);
    void clear();

    Cell& at(int r, int c) { return m_cells[r * m_cols + c]; }
    const Cell& at(int r, int c) const { return m_cells[r * m_cols + c]; }

    // Return one row as a trimmed-right string (for tests / logging).
    QString rowText(int r) const;
    // Return the whole screen as newline-joined rows (trailing blanks trimmed).
    QString toText() const;

private:
    int m_rows = 0;
    int m_cols = 0;
    QVector<Cell> m_cells;
};

} // namespace macxterm::term
