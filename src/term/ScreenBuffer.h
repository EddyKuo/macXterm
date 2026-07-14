#pragma once
#include <QString>
#include <QVector>

namespace macxterm::term {

// How a cell's foreground/background color is expressed. Default = use the
// active color scheme; Ansi = one of the 16 palette entries (scheme-controlled,
// so live-recolorable); Rgb = an absolute 24-bit color (256-color cube 16..255
// and true-color).
enum class CellColor : unsigned char { Default, Ansi, Rgb };

// A single terminal cell: one character plus SGR attributes and full color.
struct Cell {
    QChar ch = QChar(' ');
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
