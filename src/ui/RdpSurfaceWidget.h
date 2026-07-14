#pragma once
#include <QWidget>
#include <QImage>

namespace macxterm::ui {

// Render surface for RDP/VNC graphical sessions (Architecture §6.1 — gui=true
// connections render their own surface rather than a VT stream). Holds the
// remote framebuffer as a QImage and blits it, scaling to the widget. The
// FreeRDP gdi callback / VncConnection::rectDecoded feed pixels via setFrame().
class RdpSurfaceWidget : public QWidget {
    Q_OBJECT
public:
    explicit RdpSurfaceWidget(QWidget* parent = nullptr);

    // Replace the whole framebuffer.
    void setFrame(const QImage& frame);
    // Update a sub-rectangle (e.g. one decoded RDP/VNC rectangle).
    void updateRect(int x, int y, const QImage& tile);

    const QImage& frame() const { return m_frame; }
    QSize sizeHint() const override { return m_frame.size().isEmpty() ? QSize(800, 600) : m_frame.size(); }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QImage m_frame;
};

} // namespace macxterm::ui
