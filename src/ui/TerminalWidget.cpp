#include "ui/TerminalWidget.h"
#include <QPainter>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QFontMetrics>
#include <QResizeEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QApplication>
#include <QClipboard>
#include <QLineEdit>
#include <QLabel>
#include <QToolButton>
#include <QHBoxLayout>
#include <QDesktopServices>
#include <QUrl>
#include <QRegularExpression>
#include <QFile>
#include <QMessageBox>
#include <QTimer>
#include <algorithm>

namespace macxterm::ui {

TerminalWidget::TerminalWidget(QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(false);
    // Cross-platform monospace font with Nerd Font (Powerline/icon glyphs) and
    // CJK fallbacks. Primary monospace keeps its look for normal text; missing
    // glyphs fall through per-character to the Nerd Font, then CJK. Unlisted
    // fonts are simply skipped by Qt, so naming several common Nerd Fonts is safe
    // (see setTerminalFont).
    QFont f;
    f.setStyleHint(QFont::Monospace);
#if defined(Q_OS_WIN)
    f.setFamilies({"Consolas", "CaskaydiaCove Nerd Font", "JetBrainsMono Nerd Font",
                   "Symbols Nerd Font", "Microsoft YaHei Mono", "Microsoft JhengHei", "NSimSun"});
#elif defined(Q_OS_MACOS)
    f.setFamilies({"Menlo", "MesloLGS NF", "JetBrainsMono Nerd Font",
                   "Symbols Nerd Font", "PingFang SC", "Hiragino Sans"});
#else
    f.setFamilies({"DejaVu Sans Mono", "JetBrainsMono Nerd Font", "Symbols Nerd Font",
                   "Noto Sans Mono CJK SC", "Noto Sans CJK SC", "monospace"});
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
    // Re-emit terminal cwd/title so the window can update the SFTP panel/tab.
    connect(&m_vt, &term::VtEngine::cwdChanged, this, &TerminalWidget::cwdChanged);
    connect(&m_vt, &term::VtEngine::titleChanged, this, &TerminalWidget::titleChanged);
}

TerminalWidget::~TerminalWidget() { stopLogging(); }

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
    // Session logging tap: mirror raw output to the log file when enabled.
    connect(conn, &connect::IConnection::dataReceived, this, [this](const QByteArray& b) {
        if (m_logFile) { m_logFile->write(b); m_logFile->flush(); }
    });
}

void TerminalWidget::setSyntaxHighlighting(bool on) {
    if (on && m_highlighter.ruleCount() == 0) m_highlighter.loadDefaults();
    m_highlighter.setEnabled(on);
    update();
}

bool TerminalWidget::startLogging(const QString& path) {
    stopLogging();
    auto* f = new QFile(path);
    if (!f->open(QIODevice::WriteOnly | QIODevice::Append)) { delete f; return false; }
    m_logFile = f;
    return true;
}

void TerminalWidget::stopLogging() {
    if (m_logFile) { m_logFile->close(); delete m_logFile; m_logFile = nullptr; }
}

void TerminalWidget::feedInput(const QByteArray& bytes) {
    if (m_conn) m_conn->send(bytes);
}

void TerminalWidget::sendInput(const QByteArray& bytes) {
    if (m_inputHandler) m_inputHandler(bytes);   // MultiExec broadcast
    else if (m_conn) m_conn->send(bytes);
}

void TerminalWidget::recomputeGrid() {
    const int cols = std::max(1, width() / m_cellW);
    const int rows = std::max(1, height() / m_cellH);
    m_vt.resize(rows, cols);
    if (m_conn) m_conn->resize(cols, rows);
}

void TerminalWidget::resizeEvent(QResizeEvent*) {
    recomputeGrid();
    if (m_findBar && m_findBar->isVisible()) positionFindBar();
}

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

    const bool hl = m_highlighter.enabled();
    for (int r = 0; r < rows; ++r) {
        const int absLine = top + r;
        const int y = r * m_cellH;

        // Syntax highlighting: build the row text and map matched spans to an
        // per-column color override.
        QVector<QColor> hlColor;
        if (hl) {
            QString lineText(cols, QChar(' '));
            for (int c = 0; c < cols; ++c) {
                const term::Cell* cc = cellAt(absLine, c);
                if (cc) lineText[c] = term::cellUnit(cc->ch);
            }
            const auto spans = m_highlighter.highlight(lineText);
            if (!spans.isEmpty()) {
                hlColor.resize(cols);
                for (const term::HighlightSpan& s : spans)
                    for (int i = s.start; i < s.start + s.length && i < cols; ++i)
                        hlColor[i] = s.color;
            }
        }

        // Resolve a cell color kind (default/ANSI-index/RGB) to a concrete color.
        auto resolveFg = [&](const term::Cell* cell) -> QColor {
            if (!cell || cell->fgKind == term::CellColor::Default) return m_scheme.foreground();
            if (cell->fgKind == term::CellColor::Ansi) {
                // Bold brightens the low 8 ANSI colors (common terminal behavior).
                int idx = cell->fgIndex;
                if (cell->bold && idx < 8) idx += 8;
                return m_scheme.ansi(idx);
            }
            return QColor(QRgb(0xff000000u | cell->fgRgb));
        };
        auto resolveBg = [&](const term::Cell* cell) -> QColor {
            if (!cell || cell->bgKind == term::CellColor::Default) return bg;
            if (cell->bgKind == term::CellColor::Ansi) return m_scheme.ansi(cell->bgIndex);
            return QColor(QRgb(0xff000000u | cell->bgRgb));
        };

        for (int c = 0; c < cols; ++c) {
            const term::Cell* cell = cellAt(absLine, c);
            const int x = c * m_cellW;
            QColor fg = resolveFg(cell);
            if (!hlColor.isEmpty() && hlColor[c].isValid()) fg = hlColor[c];
            QColor cbg = resolveBg(cell);
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

            // A double-width (CJK) glyph spans this cell plus the next; its
            // continuation cell is blank, so paint/measure across both cells.
            const bool wide = cell && cell->wide;
            const int cw = wide ? 2 * m_cellW : m_cellW;
            if (cbg != bg) p.fillRect(x, y, cw, m_cellH, cbg);
            const char32_t cp = cell ? cell->ch : U' ';
            if (cp != U' ' && cp != 0) {
                const QString glyph = term::codePointString(cp);
                p.setPen(fg);
                if (wide)
                    p.drawText(QRect(x, y, cw, m_cellH), Qt::AlignHCenter | Qt::AlignBottom,
                               glyph);
                else
                    p.drawText(x, y + m_cellH - 3, glyph);
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
    // Find in scrollback: Cmd+F (macOS) / Ctrl+Shift+F (elsewhere).
#if defined(Q_OS_MACOS)
    const bool findCombo = cmd && e->key() == Qt::Key_F;
#else
    const bool findCombo = cmd && e->modifiers().testFlag(Qt::ShiftModifier) && e->key() == Qt::Key_F;
#endif
    if (findCombo) { showFindBar(); return; }
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
        case Qt::Key_Backspace: out = m_backspaceCode; break;
        case Qt::Key_Tab:      out = "\t"; break;
        case Qt::Key_Escape:   out = "\x1b"; break;
        case Qt::Key_Up:       out = "\x1b[A"; break;
        case Qt::Key_Down:     out = "\x1b[B"; break;
        case Qt::Key_Right:    out = "\x1b[C"; break;
        case Qt::Key_Left:     out = "\x1b[D"; break;
        default:               out = e->text().toUtf8(); break;
    }
    if (!out.isEmpty()) sendInput(out);
}

// ── mouse selection ──
QPoint TerminalWidget::cellForPos(const QPoint& pt) const {
    const int c = std::clamp(pt.x() / m_cellW, 0, m_vt.cols() - 1);
    const int r = std::clamp(pt.y() / m_cellH, 0, m_vt.rows() - 1);
    return QPoint(c, topLine() + r);
}

// Encode and transmit one mouse event to the far end using the active mouse
// encoding (default X10-style or SGR 1006). col1/row1 are 1-based.
void TerminalWidget::sendMouseReport(int cb, int col1, int row1, bool release) {
    sendInput(term::VtEngine::encodeMouseReport(m_vt.mouseEncoding(), cb, col1, row1, release));
}

// Map a widget position to a 1-based (col,row) within the visible screen and,
// if the far-end app requested mouse reporting (and Shift isn't forcing local
// selection), send the event and return true.
bool TerminalWidget::reportMouseIfEnabled(QMouseEvent* e, int cbBase, bool release, bool motion) {
    if (!m_vt.mouseEnabled()) return false;
    if (e->modifiers().testFlag(Qt::ShiftModifier)) return false;  // Shift = local override

    const auto tracking = m_vt.mouseTracking();
    if (motion) {
        // Motion only reported in button-event (with a button held) or any-motion modes.
        if (tracking == term::VtEngine::MouseTracking::AnyMotion) { /* always */ }
        else if (tracking == term::VtEngine::MouseTracking::ButtonEvent && m_mouseBtn >= 0) { /* held */ }
        else return false;
    }

    const int col1 = std::clamp(int(e->position().x()) / m_cellW, 0, m_vt.cols() - 1) + 1;
    const int row1 = std::clamp(int(e->position().y()) / m_cellH, 0, m_vt.rows() - 1) + 1;
    if (motion) {
        const QPoint cell(col1, row1);
        if (cell == m_lastMouseCell) return true;   // dedupe: same cell, no report
        m_lastMouseCell = cell;
    }

    int cb = cbBase;
    if (motion) cb += 32;                            // motion flag
    if (e->modifiers().testFlag(Qt::ShiftModifier))   cb += 4;
    if (e->modifiers().testFlag(Qt::AltModifier))     cb += 8;
    if (e->modifiers().testFlag(Qt::ControlModifier)) cb += 16;
    sendMouseReport(cb, col1, row1, release);
    return true;
}

static int buttonBaseCode(Qt::MouseButton b) {
    switch (b) {
        case Qt::LeftButton:   return 0;
        case Qt::MiddleButton: return 1;
        case Qt::RightButton:  return 2;
        default:               return 0;
    }
}

void TerminalWidget::mousePressEvent(QMouseEvent* e) {
    // Cmd (macOS) / Ctrl (elsewhere) + left click opens a URL under the cursor.
    if (e->button() == Qt::LeftButton && e->modifiers().testFlag(Qt::ControlModifier)) {
        const QPoint cell = cellForPos(e->pos());
        const QString url = urlAt(cell.y(), cell.x());
        if (!url.isEmpty()) { QDesktopServices::openUrl(QUrl(url)); return; }
    }
    const int base = buttonBaseCode(e->button());
    if (reportMouseIfEnabled(e, base, /*release=*/false, /*motion=*/false)) {
        m_mouseBtn = base;
        m_lastMouseCell = {-1, -1};
        return;
    }
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
    // Report drag/motion to the far end when it has asked for it.
    if (m_vt.mouseEnabled() && !m_selecting) {
        const int base = (m_mouseBtn >= 0) ? m_mouseBtn : 3;  // 3 = no button (any-motion)
        if (reportMouseIfEnabled(e, base, /*release=*/false, /*motion=*/true)) return;
    }
    if (m_selecting) {
        m_selHead = cellForPos(e->pos());
        m_hasSelection = (m_selHead != m_selAnchor);
        update();
    }
}

void TerminalWidget::mouseReleaseEvent(QMouseEvent* e) {
    if (m_mouseBtn >= 0 && m_vt.mouseEnabled()) {
        reportMouseIfEnabled(e, buttonBaseCode(e->button()), /*release=*/true, /*motion=*/false);
        m_mouseBtn = -1;
        return;
    }
    if (e->button() == Qt::LeftButton) {
        m_selecting = false;
        if (m_hasSelection) copySelection();   // auto-copy on select (MobaXterm-like)
    }
}

void TerminalWidget::wheelEvent(QWheelEvent* e) {
    const int steps = e->angleDelta().y() / 120;
    if (steps == 0) return;
    // When the far-end app grabs the mouse (and Shift isn't held), report wheel
    // as buttons 64 (up) / 65 (down) instead of scrolling the local scrollback.
    if (m_vt.mouseEnabled() && !e->modifiers().testFlag(Qt::ShiftModifier)) {
        const int cb = (steps > 0) ? 64 : 65;
        const int col1 = std::clamp(int(e->position().x()) / m_cellW, 0, m_vt.cols() - 1) + 1;
        const int row1 = std::clamp(int(e->position().y()) / m_cellH, 0, m_vt.rows() - 1) + 1;
        for (int i = 0; i < std::abs(steps); ++i) sendMouseReport(cb, col1, row1, false);
        return;
    }
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
            if (cell) term::appendCodePoint(row, cell->ch);
            else row.append(QChar(' '));
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
    QString text = QApplication::clipboard()->text();
    if (text.isEmpty()) return;

    // When the far-end app enabled bracketed-paste mode it can distinguish a
    // paste from typed input and won't auto-execute newlines, so the confirm
    // dialog is unnecessary; otherwise guard multi-line pastes (MobaXterm-like).
    const bool bracket = m_vt.bracketedPaste();
    const int lines = text.count('\n') + (text.endsWith('\n') ? 0 : 1);
    if (!bracket && text.contains('\n') && lines > 1) {
        const auto btn = QMessageBox::question(
            this, QStringLiteral("Confirm paste"),
            QStringLiteral("You are about to paste %1 lines. Continue?").arg(lines),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (btn != QMessageBox::Yes) return;
    }
    if (m_scrollOffset != 0) { m_scrollOffset = 0; update(); }

    if (bracket) sendInput(QByteArray("\x1b[200~"));

    // Optional paste delay: dribble multi-line pastes one line at a time.
    if (m_pasteDelayMs > 0 && text.contains('\n')) {
        const QStringList parts = text.split('\n');
        int delay = 0;
        for (int i = 0; i < parts.size(); ++i) {
            QByteArray chunk = parts[i].toUtf8();
            if (i < parts.size() - 1) chunk.append('\n');
            QTimer::singleShot(delay, this, [this, chunk] { sendInput(chunk); });
            delay += m_pasteDelayMs;
        }
        if (bracket)
            QTimer::singleShot(delay, this, [this] { sendInput(QByteArray("\x1b[201~")); });
        return;
    }
    sendInput(text.toUtf8());
    if (bracket) sendInput(QByteArray("\x1b[201~"));
}

void TerminalWidget::selectAll() {
    const int lines = totalLines();
    if (lines <= 0) return;
    m_selAnchor = QPoint(0, 0);
    m_selHead = QPoint(std::max(0, m_vt.cols() - 1), lines - 1);
    m_hasSelection = true;
    update();
}

void TerminalWidget::clearScrollback() {
    m_vt.clearScrollback();
    m_scrollOffset = 0;
    m_hasSelection = false;
    update();
}

// ── scrollback find ──
QString TerminalWidget::lineText(int absLine) const {
    QString s;
    const int cols = m_vt.cols();
    s.reserve(cols);
    for (int c = 0; c < cols; ++c) {
        const term::Cell* cell = cellAt(absLine, c);
        s.append((cell && cell->ch) ? term::cellUnit(cell->ch) : QChar(' '));
    }
    return s;
}

QString TerminalWidget::urlAt(int absLine, int col) const {
    return term::detectUrlAt(lineText(absLine), col);
}

void TerminalWidget::showFindBar() {
    if (!m_findBar) {
        m_findBar = new QWidget(this);
        m_findBar->setAutoFillBackground(true);
        auto* lay = new QHBoxLayout(m_findBar);
        lay->setContentsMargins(6, 2, 6, 2);
        lay->setSpacing(4);
        m_findEdit = new QLineEdit(m_findBar);
        m_findEdit->setPlaceholderText(QStringLiteral("Find in scrollback"));
        m_findEdit->setMaximumWidth(200);
        m_findCount = new QLabel(m_findBar);
        auto* prev = new QToolButton(m_findBar); prev->setText(QStringLiteral("▲"));
        auto* next = new QToolButton(m_findBar); next->setText(QStringLiteral("▼"));
        auto* close = new QToolButton(m_findBar); close->setText(QStringLiteral("✕"));
        lay->addWidget(m_findEdit);
        lay->addWidget(m_findCount);
        lay->addWidget(prev);
        lay->addWidget(next);
        lay->addWidget(close);
        connect(m_findEdit, &QLineEdit::textChanged, this, &TerminalWidget::findUpdate);
        connect(m_findEdit, &QLineEdit::returnPressed, this, [this] { findStep(true); });
        connect(prev, &QToolButton::clicked, this, [this] { findStep(false); });
        connect(next, &QToolButton::clicked, this, [this] { findStep(true); });
        connect(close, &QToolButton::clicked, this, [this] {
            m_findBar->hide();
            m_findMatches.clear();
            m_findIndex = -1;
            setFocus();
            update();
        });
    }
    positionFindBar();
    m_findBar->show();
    m_findBar->raise();
    m_findEdit->setFocus();
    m_findEdit->selectAll();
    if (!m_findEdit->text().isEmpty()) findUpdate(m_findEdit->text());
}

void TerminalWidget::positionFindBar() {
    if (!m_findBar) return;
    m_findBar->adjustSize();
    const int w = m_findBar->width();
    m_findBar->move(std::max(0, width() - w - 4), 4);
}

void TerminalWidget::findUpdate(const QString& query) {
    m_findIndex = -1;
    m_findLen = query.size();
    QStringList lines;
    const int total = totalLines();
    lines.reserve(total);
    for (int L = 0; L < total; ++L) lines.append(lineText(L));
    m_findMatches = term::findMatches(lines, query);   // (col, absLine) per match
    if (m_findCount) {
        m_findCount->setText(m_findMatches.isEmpty()
            ? (query.isEmpty() ? QString() : QStringLiteral("0/0"))
            : QStringLiteral("1/%1").arg(m_findMatches.size()));
    }
    if (!m_findMatches.isEmpty()) {
        // Jump to the last (most recent) match, like a shell scrollback search.
        m_findIndex = m_findMatches.size() - 1;
        findReveal(m_findIndex);
    } else {
        m_hasSelection = false;
        update();
    }
}

void TerminalWidget::findStep(bool forward) {
    if (m_findMatches.isEmpty()) return;
    m_findIndex = (m_findIndex + (forward ? 1 : -1) + m_findMatches.size()) % m_findMatches.size();
    findReveal(m_findIndex);
}

void TerminalWidget::findReveal(int index) {
    if (index < 0 || index >= m_findMatches.size()) return;
    const QPoint m = m_findMatches[index];   // (col, absLine)
    // Highlight the match via the selection machinery.
    m_selAnchor = QPoint(m.x(), m.y());
    m_selHead = QPoint(m.x() + m_findLen, m.y());
    m_hasSelection = true;
    // Scroll so the match line sits roughly mid-screen.
    const int rows = m_vt.rows();
    int offset = totalLines() - rows - m.y() + rows / 2;
    m_scrollOffset = std::clamp(offset, 0, m_vt.scrollbackCount());
    if (m_findCount)
        m_findCount->setText(QStringLiteral("%1/%2").arg(index + 1).arg(m_findMatches.size()));
    update();
}

void TerminalWidget::contextMenuEvent(QContextMenuEvent* e) {
    QMenu menu(this);
    const QPoint cell = cellForPos(e->pos());
    const QString url = urlAt(cell.y(), cell.x());
    if (!url.isEmpty()) {
        QAction* open = menu.addAction(QStringLiteral("Open Link"));
        connect(open, &QAction::triggered, this, [url] { QDesktopServices::openUrl(QUrl(url)); });
        menu.addSeparator();
    }
    QAction* copy = menu.addAction(QStringLiteral("Copy"));
    copy->setEnabled(m_hasSelection);
    connect(copy, &QAction::triggered, this, &TerminalWidget::copySelection);

    QAction* paste = menu.addAction(QStringLiteral("Paste"));
    paste->setEnabled(!QApplication::clipboard()->text().isEmpty());
    connect(paste, &QAction::triggered, this, &TerminalWidget::paste);

    QAction* selectAllAct = menu.addAction(QStringLiteral("Select All"));
    connect(selectAllAct, &QAction::triggered, this, &TerminalWidget::selectAll);

    QAction* findAct = menu.addAction(QStringLiteral("Find…"));
    connect(findAct, &QAction::triggered, this, &TerminalWidget::showFindBar);

    menu.addSeparator();
    QAction* clear = menu.addAction(QStringLiteral("Clear Scrollback"));
    clear->setEnabled(m_vt.scrollbackCount() > 0);
    connect(clear, &QAction::triggered, this, &TerminalWidget::clearScrollback);

    menu.exec(e->globalPos());
}

} // namespace macxterm::ui
