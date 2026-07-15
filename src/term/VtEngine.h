#pragma once
#include "term/ScreenBuffer.h"
#include <QObject>
#include <QByteArray>
#include <QList>
#include <QVector>

struct VTerm;
struct VTermScreen;

namespace macxterm::term {

// VT100/VT220/xterm terminal emulation backed by libvterm (MIT, ADR_6).
// Feed remote/PTY bytes via input(); the ScreenBuffer reflects the render.
// Emits outputReady() when the emulator needs to send bytes back (e.g. replies
// to device-status queries) — wire this to the connection's write().
class VtEngine : public QObject {
    Q_OBJECT
public:
    explicit VtEngine(int rows = 24, int cols = 80, QObject* parent = nullptr);
    ~VtEngine() override;

    void resize(int rows, int cols);
    void input(const QByteArray& bytes);   // feed data from the far end
    void reset();

    int rows() const { return m_screen.rows(); }
    int cols() const { return m_screen.cols(); }
    const ScreenBuffer& screen() const { return m_screen; }

    // Convenience: the current visible screen as text.
    QString screenText() const { return m_screen.toText(); }

    // Scrollback: lines that have scrolled off the top of the screen, oldest
    // first. Index 0 is the oldest retained line.
    int scrollbackCount() const { return m_scrollback.size(); }
    // Cap on retained scrolled-off lines. Trims immediately if the new cap is
    // smaller than the current backlog. A value <= 0 disables scrollback.
    void setScrollbackMax(int lines) {
        m_scrollbackMax = lines < 0 ? 0 : lines;
        while (m_scrollback.size() > m_scrollbackMax) m_scrollback.removeFirst();
    }
    int scrollbackMax() const { return m_scrollbackMax; }
    // Drop all scrolled-off history (the visible screen is left untouched).
    void clearScrollback() { m_scrollback.clear(); }
    const QVector<Cell>& scrollbackLine(int i) const { return m_scrollback.at(i); }

    // Bracketed-paste mode (DECSET 2004): when the far-end app has enabled it,
    // pasted text must be wrapped in ESC[200~ … ESC[201~ so the app can tell a
    // paste from typed input.
    bool bracketedPaste() const { return m_bracketedPaste; }

signals:
    void outputReady(const QByteArray& bytes);
    void screenUpdated();
    void titleChanged(const QString& title);   // OSC 0/2
    void cwdChanged(const QString& absPath);    // OSC 7 (file://host/path)

private:
    void syncFromVterm();
    void scanOsc(const QByteArray& bytes);      // sniff OSC 7 cwd from raw stream
    void scanPrivateModes(const QByteArray& bytes);  // sniff DECSET/DECRST (e.g. 2004)

    VTerm* m_vt = nullptr;
    VTermScreen* m_vts = nullptr;
    ScreenBuffer m_screen;
    QByteArray m_pendingOutput;
    QList<QVector<Cell>> m_scrollback;   // scrolled-off lines (capped)
    int m_scrollbackMax = 10000;
    // OSC scanner state (for OSC 7 cwd / OSC 0,2 title).
    int m_oscState = 0;                  // 0 normal, 1 saw ESC, 2 in OSC body
    QByteArray m_oscBuf;
    // CSI private-mode scanner state (for DECSET/DECRST like 2004 bracketed paste).
    int m_csiState = 0;                  // 0 normal, 1 saw ESC, 2 saw '[', 3 in '?...' body
    QByteArray m_csiParams;
    bool m_bracketedPaste = false;
    friend void vt_output_cb(const char* s, size_t len, void* user);
    friend int vt_sb_pushline(int cols, const void* cells, void* user);
    friend int vt_sb_popline(int cols, void* cells, void* user);
};

} // namespace macxterm::term
