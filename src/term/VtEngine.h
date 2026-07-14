#pragma once
#include "term/ScreenBuffer.h"
#include <QObject>
#include <QByteArray>

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

signals:
    void outputReady(const QByteArray& bytes);
    void screenUpdated();

private:
    void syncFromVterm();

    VTerm* m_vt = nullptr;
    VTermScreen* m_vts = nullptr;
    ScreenBuffer m_screen;
    QByteArray m_pendingOutput;
    friend void vt_output_cb(const char* s, size_t len, void* user);
};

} // namespace macxterm::term
