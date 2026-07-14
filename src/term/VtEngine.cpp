#include "term/VtEngine.h"
#include <vterm.h>
#include <QUrl>
#include <QString>

namespace macxterm::term {

namespace {
// Resolve one libvterm cell color into our CellColor representation. Indexed
// 0..15 stay as scheme-controlled ANSI indices; the 256-color cube (16..255) and
// true-color are baked to absolute RGB.
void mapColor(const VTermColor& col, CellColor& kind, unsigned char& index, unsigned int& rgb) {
    if (VTERM_COLOR_IS_DEFAULT_FG(&col) || VTERM_COLOR_IS_DEFAULT_BG(&col)) {
        kind = CellColor::Default;
        return;
    }
    if (VTERM_COLOR_IS_INDEXED(&col)) {
        const unsigned idx = col.indexed.idx;
        if (idx < 16) { kind = CellColor::Ansi; index = static_cast<unsigned char>(idx); return; }
        // xterm 256-color palette → RGB.
        unsigned r = 0, g = 0, b = 0;
        if (idx < 232) {
            const unsigned c = idx - 16;
            const unsigned lut[6] = {0, 95, 135, 175, 215, 255};
            r = lut[(c / 36) % 6]; g = lut[(c / 6) % 6]; b = lut[c % 6];
        } else {
            r = g = b = 8 + 10 * (idx - 232);
        }
        kind = CellColor::Rgb;
        rgb = (r << 16) | (g << 8) | b;
        return;
    }
    // RGB / true-color.
    kind = CellColor::Rgb;
    rgb = (unsigned(col.rgb.red) << 16) | (unsigned(col.rgb.green) << 8) | unsigned(col.rgb.blue);
}
} // namespace

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
        cell.wide = (cells[c].width == 2);
        mapColor(cells[c].fg, cell.fgKind, cell.fgIndex, cell.fgRgb);
        mapColor(cells[c].bg, cell.bgKind, cell.bgIndex, cell.bgRgb);
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
        c.settermprop = [](VTermProp prop, VTermValue* val, void* user) -> int {
            if (prop == VTERM_PROP_TITLE && val) {
                auto* self = static_cast<VtEngine*>(user);
#if defined(VTERM_VERSION_MAJOR) && (VTERM_VERSION_MAJOR > 0 || VTERM_VERSION_MINOR >= 2)
                // libvterm >= 0.2 delivers strings as fragments.
                const QString t = QString::fromUtf8(val->string.str, val->string.len);
#else
                const QString t = QString::fromUtf8(val->string);
#endif
                emit self->titleChanged(t);
            }
            return 1;
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

// Scan the raw stream for OSC 7 (current working directory), which libvterm
// does not surface as a term property. A shell emits: ESC ] 7 ; file://host/path
// terminated by BEL (0x07) or ST (ESC \). We keep a small state machine so a
// sequence may span multiple input() calls.
void VtEngine::scanOsc(const QByteArray& bytes) {
    for (int i = 0; i < bytes.size(); ++i) {
        const unsigned char b = static_cast<unsigned char>(bytes[i]);
        switch (m_oscState) {
        case 0:
            if (b == 0x1b) m_oscState = 1;
            break;
        case 1:
            if (b == ']') { m_oscState = 2; m_oscBuf.clear(); }
            else m_oscState = 0;
            break;
        case 2:
            if (b == 0x07 || b == 0x1b) {          // BEL or start of ST (ESC \)
                // Parse "cmd;payload".
                const int semi = m_oscBuf.indexOf(';');
                if (semi > 0) {
                    const QByteArray cmd = m_oscBuf.left(semi);
                    const QByteArray payload = m_oscBuf.mid(semi + 1);
                    if (cmd == "7") {
                        const QUrl url = QUrl(QString::fromUtf8(payload));
                        const QString path = url.path();
                        if (!path.isEmpty()) emit cwdChanged(path);
                    }
                }
                m_oscBuf.clear();
                m_oscState = (b == 0x1b) ? 1 : 0;   // ESC may begin ST's backslash
            } else {
                if (m_oscBuf.size() < 4096) m_oscBuf.append(static_cast<char>(b));
            }
            break;
        }
    }
}

void VtEngine::input(const QByteArray& bytes) {
    scanOsc(bytes);
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
                cell.wide = (vc.width == 2);
                mapColor(vc.fg, cell.fgKind, cell.fgIndex, cell.fgRgb);
                mapColor(vc.bg, cell.bgKind, cell.bgIndex, cell.bgRgb);
            }
            m_screen.at(r, c) = cell;
        }
    }
    emit screenUpdated();
}

} // namespace macxterm::term
