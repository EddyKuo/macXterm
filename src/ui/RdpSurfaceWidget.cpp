#include "ui/RdpSurfaceWidget.h"
#include <QPainter>

namespace macxterm::ui {

RdpSurfaceWidget::RdpSurfaceWidget(QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void RdpSurfaceWidget::setFrame(const QImage& frame) {
    m_frame = frame;
    update();
}

void RdpSurfaceWidget::updateRect(int x, int y, const QImage& tile) {
    if (m_frame.isNull()) { m_frame = tile.copy(); return; }
    QPainter p(&m_frame);
    p.drawImage(x, y, tile);
    update(QRect(x, y, tile.width(), tile.height()));
}

void RdpSurfaceWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    if (m_frame.isNull()) {
        p.fillRect(rect(), Qt::black);
        return;
    }
    // Aspect-preserving blit, scaled to the widget.
    p.drawImage(rect(), m_frame, m_frame.rect());
}

} // namespace macxterm::ui
