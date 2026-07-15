#pragma once
#include "connect/IConnection.h"
#include <QStringList>
#include <QImage>

struct rdp_freerdp;   // FreeRDP instance (opaque here)

namespace macxterm::connect {

// RDP session (research §1.1). Rendering uses FreeRDP (Apache-2.0, ADR_6) when
// the library is available at build time (MACXTERM_HAVE_FREERDP). RDP renders
// its own graphics surface, so capabilities().gui = true and no VT stream flows
// through the terminal — the UI hosts a dedicated surface widget.
//
// buildFreeRdpArgs() is pure and unit-tested regardless of whether FreeRDP is
// linked, so the connection-parameter mapping is verified in all builds.
class RdpConnection : public IConnection {
    Q_OBJECT
public:
    static QStringList buildFreeRdpArgs(const core::Session& session);

    explicit RdpConnection(QObject* parent = nullptr);

    bool connectSession(const core::Session& session) override;
    void disconnectSession() override;
    qint64 send(const QByteArray& data) override;
    Capabilities capabilities() const override { return {false, false, true, /*gui*/true}; }

    // Wrap the current FreeRDP gdi framebuffer as a QImage (the render surface
    // source, fed to RdpSurfaceWidget). Empty when not connected / no FreeRDP.
    QImage currentFrame() const;

    // Pump one iteration of the FreeRDP event loop (call from a timer/thread).
    // Returns false when the session has ended. No-op without FreeRDP.
    bool poll();

    // Input injection from the render surface. Coordinates are in framebuffer
    // pixels; buttonMask bit0 left, bit1 middle, bit2 right; keysym is an X11
    // keysym (printable chars are sent as Unicode). No-ops without FreeRDP.
    void sendPointerEvent(int x, int y, int buttonMask);
    void sendKeyEvent(quint32 keysym, bool down);

private:
    rdp_freerdp* m_instance = nullptr;   // only used when built with FreeRDP
    int m_prevButtonMask = 0;            // for mouse button transition detection
};

} // namespace macxterm::connect
