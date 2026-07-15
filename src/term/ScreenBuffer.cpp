#include "term/ScreenBuffer.h"
#include <QStringList>

namespace macxterm::term {

void ScreenBuffer::resize(int rows, int cols) {
    m_rows = rows > 0 ? rows : 1;
    m_cols = cols > 0 ? cols : 1;
    m_cells.fill(Cell{}, m_rows * m_cols);   // fill(value,size) — portable across Qt6 (assign() needs 6.6+)
}

void ScreenBuffer::clear() {
    for (Cell& c : m_cells) c = Cell{};
}

QString ScreenBuffer::rowText(int r) const {
    if (r < 0 || r >= m_rows) return {};
    QString line;
    line.reserve(m_cols);
    for (int c = 0; c < m_cols; ++c) appendCodePoint(line, at(r, c).ch);
    while (!line.isEmpty() && line.back() == QChar(' ')) line.chop(1);
    return line;
}

QString ScreenBuffer::toText() const {
    QStringList lines;
    for (int r = 0; r < m_rows; ++r) lines << rowText(r);
    while (!lines.isEmpty() && lines.back().isEmpty()) lines.removeLast();
    return lines.join('\n');
}

} // namespace macxterm::term
