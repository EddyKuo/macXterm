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
    const QVector<Cell>& scrollbackLine(int i) const { return m_scrollback.at(i); }

signals:
    void outputReady(const QByteArray& bytes);
    void screenUpdated();

private:
    void syncFromVterm();

    VTerm* m_vt = nullptr;
    VTermScreen* m_vts = nullptr;
    ScreenBuffer m_screen;
    QByteArray m_pendingOutput;
    QList<QVector<Cell>> m_scrollback;   // scrolled-off lines (capped)
    int m_scrollbackMax = 10000;
    friend void vt_output_cb(const char* s, size_t len, void* user);
    friend int vt_sb_pushline(int cols, const void* cells, void* user);
    friend int vt_sb_popline(int cols, void* cells, void* user);
};

} // namespace macxterm::term
