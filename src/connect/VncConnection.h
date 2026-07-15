#pragma once
#include "connect/IConnection.h"
#include "connect/RfbProtocol.h"
#include <QList>

class QTcpSocket;

namespace macxterm::connect {

// VNC (RFB 3.8) client built on the MIT self-implemented RfbProtocol codec
// (ADR_6 — no GPL libvncclient). Drives the handshake state machine over a
// socket and decodes framebuffer updates into ARGB pixels. GUI-surface session:
// capabilities().gui = true. Runnable end-to-end over loopback.
class VncConnection : public IConnection {
    Q_OBJECT
public:
    explicit VncConnection(QObject* parent = nullptr);

    bool connectSession(const core::Session& session) override;
    void disconnectSession() override;
    qint64 send(const QByteArray& data) override;
    Capabilities capabilities() const override { return {false, false, true, /*gui*/true}; }

    int framebufferWidth() const { return m_serverInit.width; }
    int framebufferHeight() const { return m_serverInit.height; }

    // Input injection (RFB PointerEvent / KeyEvent). No-ops in view-only mode or
    // before the session is running. buttonMask: bit0 left, bit1 middle, bit2 right.
    void sendPointerEvent(int x, int y, int buttonMask);
    void sendKeyEvent(quint32 keysym, bool down);
    void setViewOnly(bool on) { m_viewOnly = on; }
    bool viewOnly() const { return m_viewOnly; }

signals:
    // Emitted when a rectangle is decoded (x, y, w, h, ARGB pixels row-major).
    void rectDecoded(int x, int y, int w, int h, const QList<quint32>& pixels);
    void serverReady(int width, int height, const QString& name);

private slots:
    void onReadyRead();

private:
    enum class Phase { Version, Security, SecurityResult, ServerInit, Running };

    void requestFramebuffer();

    QTcpSocket* m_sock = nullptr;
    Phase m_phase = Phase::Version;
    QByteArray m_buf;
    rfb::ServerInit m_serverInit;
    bool m_viewOnly = false;
    int m_buttonMask = 0;   // last pointer button state (for key-with-buttons)
};

} // namespace macxterm::connect
