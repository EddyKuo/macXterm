#include "ui/TerminalWidget.h"
#include <QPainter>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QFontMetrics>
#include <QResizeEvent>
#include <QApplication>
#include <QClipboard>
#include <algorithm>

namespace macxterm::ui {

TerminalWidget::TerminalWidget(QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(false);
    // Cross-platform monospace font with CJK fallbacks (see setTerminalFont).
    QFont f;
    f.setStyleHint(QFont::Monospace);
#if defined(Q_OS_WIN)
    f.setFamilies({"Consolas", "Microsoft YaHei Mono", "Microsoft JhengHei", "NSimSun"});
#elif defined(Q_OS_MACOS)
    f.setFamilies({"Menlo", "PingFang SC", "Hiragino Sans"});
#else
    f.setFamilies({"DejaVu Sans Mono", "Noto Sans Mono CJK SC", "Noto Sans CJK SC", "monospace"});
#endif
    f.setFixedPitch(true);
    f.setPointSize(12);
    setFont(f);
    updateCellMetrics();
    setAutoFillBackground(true);
    connect(&m_vt, &term::VtEngine::screenUpdated, this, [this] {
        // Follow the live output only when already at the bottom.
        if (m_scrollOffset == 0) update();
        else update();
    });
}

void TerminalWidget::updateCellMetrics() {
    QFontMetrics fm(font());
    m_cellW = std::max(1, fm.horizontalAdvance('M'));
    m_cellH = std::max(1, fm.height());
}

void TerminalWidget::setTerminalFont(const QFont& fontIn) {
    QFont f = fontIn;
    f.setFixedPitch(true);
    setFont(f);
    updateCellMetrics();
    recomputeGrid();
    update();
}

void TerminalWidget::setColorScheme(const term::ColorScheme& scheme) {
    m_scheme = scheme;
    update();
}

void TerminalWidget::attach(connect::IConnection* conn) {
    m_conn = conn;
    connect(conn, &connect::IConnection::dataReceived, &m_vt, &term::VtEngine::input);
    connect(&m_vt, &term::VtEngine::outputReady, conn, &connect::IConnection::send);
}

void TerminalWidget::feedInput(const QByteArray& bytes) {
    if (m_conn) m_conn->send(bytes);
}

void TerminalWidget::recomputeGrid() {
    const int cols = std::max(1, width() / m_cellW);
    const int rows = std::max(1, height() / m_cellH);
    m_vt.resize(rows, cols);
    if (m_conn) m_conn->resize(cols, rows);
}

void TerminalWidget::resizeEvent(QResizeEvent*) { recomputeGrid(); }

// ── line addressing across scrollback + live screen ──
int TerminalWidget::totalLines() const { return m_vt.scrollbackCount() + m_vt.rows(); }

int TerminalWidget::topLine() const {
    const int rows = m_vt.rows();
    int top = totalLines() - rows - m_scrollOffset;
    return std::max(0, top);
}

const term::Cell* TerminalWidget::cellAt(int absLine, int col) const {
    const int sb = m_vt.scrollbackCount();
    if (absLine < sb) {
        const QVector<term::Cell>& line = m_vt.scrollbackLine(absLine);
        return (col < line.size()) ? &line[col] : nullptr;
    }
    const int r = absLine - sb;
    if (r < 0 || r >= m_vt.screen().rows() || col >= m_vt.screen().cols()) return nullptr;
    return &m_vt.screen().at(r, col);
}

// ── rendering ──
void TerminalWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    const QColor bg = m_scheme.background();
    p.fillRect(rect(), bg);
    p.setFont(font());

    const int rows = m_vt.rows();
    const int cols = m_vt.cols();
    const int top = topLine();

    // Selection bounds (normalized, absolute line coords).
    auto selNormalized = [&](QPoint& a, QPoint& b) {
        a = m_selAnchor; b = m_selHead;
        if (a.y() > b.y() || (a.y() == b.y() && a.x() > b.x())) std::swap(a, b);
    };
    QPoint sa, sb2;
    if (m_hasSelection) selNormalized(sa, sb2);

    for (int r = 0; r < rows; ++r) {
        const int absLine = top + r;
        const int y = r * m_cellH;
        for (int c = 0; c < cols; ++c) {
            const term::Cell* cell = cellAt(absLine, c);
            const int x = c * m_cellW;
            QColor fg = m_scheme.ansi(cell ? (cell->fg & 0x0f) : 7);
            QColor cbg = m_scheme.ansi(cell ? (cell->bg & 0x0f) : 0);
            if (cbg == m_scheme.ansi(0)) cbg = bg;
            bool reverse = cell && cell->reverse;

            // Is this cell inside the selection?
            bool sel = false;
            if (m_hasSelection) {
                const bool afterStart = (absLine > sa.y()) || (absLine == sa.y() && c >= sa.x());
                const bool beforeEnd  = (absLine < sb2.y()) || (absLine == sb2.y() && c < sb2.x());
                sel = afterStart && beforeEnd;
            }
            if (reverse) std::swap(fg, cbg);
            if (sel) { std::swap(fg, cbg); }

            if (cbg != bg) p.fillRect(x, y, m_cellW, m_cellH, cbg);
            const QChar ch = cell ? cell->ch : QChar(' ');
            if (ch != QChar(' ')) {
                p.setPen(fg);
                p.drawText(x, y + m_cellH - 3, QString(ch));
            }
        }
    }

    // Scroll indicator when not at the bottom.
    if (m_scrollOffset > 0) {
        p.fillRect(width() - 3, 0, 3, height(), QColor(120, 120, 120, 120));
    }
}

// ── keyboard ──
void TerminalWidget::keyPressEvent(QKeyEvent* e) {
    // Copy / paste (Cmd on macOS, Ctrl+Shift elsewhere; Shift+Ins also pastes).
    const bool cmd = e->modifiers().testFlag(Qt::ControlModifier);
#if defined(Q_OS_MACOS)
    const bool copyCombo = cmd && e->key() == Qt::Key_C && m_hasSelection;
    const bool pasteCombo = cmd && e->key() == Qt::Key_V;
#else
    const bool copyCombo = cmd && e->modifiers().testFlag(Qt::ShiftModifier) && e->key() == Qt::Key_C && m_hasSelection;
    const bool pasteCombo = (cmd && e->modifiers().testFlag(Qt::ShiftModifier) && e->key() == Qt::Key_V);
#endif
    if (copyCombo) { copySelection(); return; }
    if (pasteCombo || (e->key() == Qt::Key_Insert && e->modifiers().testFlag(Qt::ShiftModifier))) {
        paste(); return;
    }
    // Page-up/down scrolls the scrollback when Shift is held.
    if (e->modifiers().testFlag(Qt::ShiftModifier) &&
        (e->key() == Qt::Key_PageUp || e->key() == Qt::Key_PageDown)) {
        const int page = m_vt.rows();
        m_scrollOffset += (e->key() == Qt::Key_PageUp) ? page : -page;
        m_scrollOffset = std::clamp(m_scrollOffset, 0, m_vt.scrollbackCount());
        update();
        return;
    }

    if (!m_conn) return;
    // Any typing jumps back to the live view.
    if (m_scrollOffset != 0) { m_scrollOffset = 0; update(); }

    QByteArray out;
    switch (e->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter:    out = "\r"; break;
        case Qt::Key_Backspace: out = "\x7f"; break;
        case Qt::Key_Tab:      out = "\t"; break;
        case Qt::Key_Escape:   out = "\x1b"; break;
        case Qt::Key_Up:       out = "\x1b[A"; break;
        case Qt::Key_Down:     out = "\x1b[B"; break;
        case Qt::Key_Right:    out = "\x1b[C"; break;
        case Qt::Key_Left:     out = "\x1b[D"; break;
        default:               out = e->text().toUtf8(); break;
    }
    if (!out.isEmpty()) m_conn->send(out);
}

// ── mouse selection ──
QPoint TerminalWidget::cellForPos(const QPoint& pt) const {
    const int c = std::clamp(pt.x() / m_cellW, 0, m_vt.cols() - 1);
    const int r = std::clamp(pt.y() / m_cellH, 0, m_vt.rows() - 1);
    return QPoint(c, topLine() + r);
}

void TerminalWidget::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        m_selecting = true;
        m_hasSelection = false;
        m_selAnchor = m_selHead = cellForPos(e->pos());
        update();
    } else if (e->button() == Qt::MiddleButton) {
        paste();   // X11-style middle-click paste
    }
}

void TerminalWidget::mouseMoveEvent(QMouseEvent* e) {
    if (m_selecting) {
        m_selHead = cellForPos(e->pos());
        m_hasSelection = (m_selHead != m_selAnchor);
        update();
    }
}

void TerminalWidget::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        m_selecting = false;
        if (m_hasSelection) copySelection();   // auto-copy on select (MobaXterm-like)
    }
}

void TerminalWidget::wheelEvent(QWheelEvent* e) {
    const int steps = e->angleDelta().y() / 120;
    if (steps == 0) return;
    m_scrollOffset += steps * 3;   // 3 lines per wheel notch
    m_scrollOffset = std::clamp(m_scrollOffset, 0, m_vt.scrollbackCount());
    update();
}

// ── clipboard ──
QString TerminalWidget::selectedText() const {
    if (!m_hasSelection) return {};
    QPoint a = m_selAnchor, b = m_selHead;
    if (a.y() > b.y() || (a.y() == b.y() && a.x() > b.x())) std::swap(a, b);
    QString text;
    for (int line = a.y(); line <= b.y(); ++line) {
        const int c0 = (line == a.y()) ? a.x() : 0;
        const int c1 = (line == b.y()) ? b.x() : m_vt.cols();
        QString row;
        for (int c = c0; c < c1; ++c) {
            const term::Cell* cell = cellAt(line, c);
            row.append(cell ? cell->ch : QChar(' '));
        }
        while (!row.isEmpty() && row.back() == QChar(' ')) row.chop(1);  // trim trailing
        text.append(row);
        if (line != b.y()) text.append('\n');
    }
    return text;
}

void TerminalWidget::copySelection() {
    const QString text = selectedText();
    if (!text.isEmpty()) QApplication::clipboard()->setText(text);
}

void TerminalWidget::paste() {
    if (!m_conn) return;
    const QString text = QApplication::clipboard()->text();
    if (!text.isEmpty()) {
        if (m_scrollOffset != 0) { m_scrollOffset = 0; update(); }
        m_conn->send(text.toUtf8());
    }
}

} // namespace macxterm::ui
