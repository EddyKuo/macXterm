#include "term/VtEngine.h"
#include <vterm.h>

namespace macxterm::term {

// libvterm calls this when the emulator produces output to send back.
void vt_output_cb(const char* s, size_t len, void* user) {
    auto* self = static_cast<VtEngine*>(user);
    self->m_pendingOutput.append(s, static_cast<int>(len));
}

// Scrollback: libvterm calls sb_pushline when a line scrolls off the top.
int vt_sb_pushline(int cols, const void* cellsv, void* user) {
    auto* self = static_cast<VtEngine*>(user);
    const auto* cells = static_cast<const VTermScreenCell*>(cellsv);
    QVector<Cell> line(cols);
    for (int c = 0; c < cols; ++c) {
        const uint32_t cp = cells[c].chars[0];
        Cell cell;
        if (cp != 0 && cp != static_cast<uint32_t>(-1))
            cell.ch = (cp <= 0xFFFF) ? QChar(static_cast<char16_t>(cp))
                                     : QChar(QChar::ReplacementCharacter);
        cell.bold = cells[c].attrs.bold;
        cell.reverse = cells[c].attrs.reverse;
        line[c] = cell;
    }
    self->m_scrollback.push_back(line);
    while (self->m_scrollback.size() > self->m_scrollbackMax)
        self->m_scrollback.removeFirst();
    return 1;
}

// libvterm calls sb_popline to restore a previously-scrolled-off line.
int vt_sb_popline(int cols, void* cellsv, void* user) {
    auto* self = static_cast<VtEngine*>(user);
    if (self->m_scrollback.isEmpty()) return 0;
    auto* cells = static_cast<VTermScreenCell*>(cellsv);
    const QVector<Cell> line = self->m_scrollback.takeLast();
    for (int c = 0; c < cols; ++c) {
        cells[c].chars[0] = (c < line.size()) ? line[c].ch.unicode() : 0;
        cells[c].chars[1] = 0;
        cells[c].width = 1;
    }
    return 1;
}

VtEngine::VtEngine(int rows, int cols, QObject* parent)
    : QObject(parent), m_screen(rows, cols) {
    m_vt = vterm_new(rows, cols);
    vterm_set_utf8(m_vt, 1);
    vterm_output_set_callback(m_vt, vt_output_cb, this);
    m_vts = vterm_obtain_screen(m_vt);

    // Register scrollback callbacks so scrolled-off lines are retained.
    static const VTermScreenCallbacks kCallbacks = [] {
        VTermScreenCallbacks c{};
        c.sb_pushline = [](int cols, const VTermScreenCell* cells, void* user) {
            return vt_sb_pushline(cols, cells, user);
        };
        c.sb_popline = [](int cols, VTermScreenCell* cells, void* user) {
            return vt_sb_popline(cols, cells, user);
        };
        return c;
    }();
    vterm_screen_set_callbacks(m_vts, &kCallbacks, this);

    vterm_screen_enable_altscreen(m_vts, 1);
    vterm_screen_reset(m_vts, 1);
}

VtEngine::~VtEngine() {
    if (m_vt) vterm_free(m_vt);
}

void VtEngine::reset() {
    vterm_screen_reset(m_vts, 1);
    m_screen.clear();
    emit screenUpdated();
}

void VtEngine::resize(int rows, int cols) {
    vterm_set_size(m_vt, rows, cols);
    m_screen.resize(rows, cols);
    syncFromVterm();
}

void VtEngine::input(const QByteArray& bytes) {
    vterm_input_write(m_vt, bytes.constData(), bytes.size());
    if (!m_pendingOutput.isEmpty()) {
        emit outputReady(m_pendingOutput);
        m_pendingOutput.clear();
    }
    syncFromVterm();
}

void VtEngine::syncFromVterm() {
    const int rows = m_screen.rows();
    const int cols = m_screen.cols();
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            VTermPos pos; pos.row = r; pos.col = c;
            VTermScreenCell vc;
            Cell cell;
            // Skip empty cells and the right-hand continuation cell of a
            // double-width (CJK) glyph — libvterm marks the latter with
            // chars[0] == (uint32_t)-1. Treating it as a code point produced a
            // U+FFFD after every wide character (mojibake).
            if (vterm_screen_get_cell(m_vts, pos, &vc)
                && vc.chars[0] != 0
                && vc.chars[0] != static_cast<uint32_t>(-1)) {
                const uint32_t cp = vc.chars[0];
                // ScreenBuffer holds one UTF-16 unit per cell; non-BMP code
                // points (> 0xFFFF) can't fit — QChar(char32_t) would assert —
                // so substitute the replacement character. Full astral-plane
                // support would widen Cell to a QString (Phase 6).
                cell.ch = (cp <= 0xFFFF) ? QChar(static_cast<char16_t>(cp))
                                         : QChar(QChar::ReplacementCharacter);
                cell.bold = vc.attrs.bold;
                cell.reverse = vc.attrs.reverse;
            }
            m_screen.at(r, c) = cell;
        }
    }
    emit screenUpdated();
}

} // namespace macxterm::term
