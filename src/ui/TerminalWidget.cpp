#include "ui/TerminalWidget.h"
#include <QPainter>
#include <QKeyEvent>
#include <QFontMetrics>
#include <QResizeEvent>

namespace macxterm::ui {

// ANSI 16-color palette (xterm defaults) for basic rendering.
static const QColor kAnsi[16] = {
    QColor(0,0,0),       QColor(205,0,0),    QColor(0,205,0),    QColor(205,205,0),
    QColor(0,0,238),     QColor(205,0,205),  QColor(0,205,205),  QColor(229,229,229),
    QColor(127,127,127), QColor(255,0,0),    QColor(0,255,0),    QColor(255,255,0),
    QColor(92,92,255),   QColor(255,0,255),  QColor(0,255,255),  QColor(255,255,255),
};

TerminalWidget::TerminalWidget(QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    // Cross-platform monospace font with CJK fallbacks. A hard-coded "Menlo"
    // (macOS-only) falls back to a font with no CJK glyphs on Windows/Linux,
    // rendering Chinese/Japanese/Korean as tofu/garbage. Qt walks this family
    // list per glyph, so a Latin monospace face + a CJK face together cover both.
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
    QFontMetrics fm(f);
    m_cellW = fm.horizontalAdvance('M');
    m_cellH = fm.height();
    setAutoFillBackground(true);
    connect(&m_vt, &term::VtEngine::screenUpdated, this,
            QOverload<>::of(&QWidget::update));
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
    const int cols = qMax(1, width() / m_cellW);
    const int rows = qMax(1, height() / m_cellH);
    m_vt.resize(rows, cols);
    if (m_conn) m_conn->resize(cols, rows);
}

void TerminalWidget::resizeEvent(QResizeEvent*) { recomputeGrid(); }

void TerminalWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), kAnsi[0]);
    p.setFont(font());
    const auto& screen = m_vt.screen();
    for (int r = 0; r < screen.rows(); ++r) {
        for (int c = 0; c < screen.cols(); ++c) {
            const auto& cell = screen.at(r, c);
            const int x = c * m_cellW;
            const int y = r * m_cellH;
            QColor fg = kAnsi[cell.fg & 0x0f];
            QColor bg = kAnsi[cell.bg & 0x0f];
            if (cell.reverse) std::swap(fg, bg);
            if (bg != kAnsi[0]) p.fillRect(x, y, m_cellW, m_cellH, bg);
            if (cell.ch != QChar(' ')) {
                p.setPen(fg);
                p.drawText(x, y + m_cellH - 3, QString(cell.ch));
            }
        }
    }
}

void TerminalWidget::keyPressEvent(QKeyEvent* e) {
    if (!m_conn) return;
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

} // namespace macxterm::ui
