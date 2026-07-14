#include "term/VtEngine.h"
#include <vterm.h>

namespace macxterm::term {

// libvterm calls this when the emulator produces output to send back.
void vt_output_cb(const char* s, size_t len, void* user) {
    auto* self = static_cast<VtEngine*>(user);
    self->m_pendingOutput.append(s, static_cast<int>(len));
}

VtEngine::VtEngine(int rows, int cols, QObject* parent)
    : QObject(parent), m_screen(rows, cols) {
    m_vt = vterm_new(rows, cols);
    vterm_set_utf8(m_vt, 1);
    vterm_output_set_callback(m_vt, vt_output_cb, this);
    m_vts = vterm_obtain_screen(m_vt);
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
            if (vterm_screen_get_cell(m_vts, pos, &vc) && vc.chars[0] != 0) {
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
