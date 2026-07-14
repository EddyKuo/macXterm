#pragma once
#include <QString>
#include <QVector>

namespace macxterm::term {

// A single terminal cell: one character plus basic SGR attributes.
struct Cell {
    QChar ch = QChar(' ');
    unsigned char fg = 7;   // default foreground (ANSI index)
    unsigned char bg = 0;   // default background
    bool bold = false;
    bool reverse = false;
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
