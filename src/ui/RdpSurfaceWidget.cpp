#include "ui/RdpSurfaceWidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>

namespace macxterm::ui {

namespace {
// Map a Qt key event to an X11 keysym for RFB KeyEvent. Printable characters use
// their Unicode value directly (Latin-1 range maps 1:1); named keys use the
// standard X keysym constants.
quint32 qtKeyToKeysym(const QKeyEvent* e) {
    switch (e->key()) {
        case Qt::Key_Backspace: return 0xff08;
        case Qt::Key_Tab:       return 0xff09;
        case Qt::Key_Return:
        case Qt::Key_Enter:     return 0xff0d;
        case Qt::Key_Escape:    return 0xff1b;
        case Qt::Key_Delete:    return 0xffff;
        case Qt::Key_Home:      return 0xff50;
        case Qt::Key_Left:      return 0xff51;
        case Qt::Key_Up:        return 0xff52;
        case Qt::Key_Right:     return 0xff53;
        case Qt::Key_Down:      return 0xff54;
        case Qt::Key_PageUp:    return 0xff55;
        case Qt::Key_PageDown:  return 0xff56;
        case Qt::Key_End:       return 0xff57;
        case Qt::Key_Insert:    return 0xff63;
        case Qt::Key_Shift:     return 0xffe1;
        case Qt::Key_Control:   return 0xffe3;
        case Qt::Key_Alt:       return 0xffe9;
        case Qt::Key_Meta:      return 0xffeb;
        default: break;
    }
    if (e->key() >= Qt::Key_F1 && e->key() <= Qt::Key_F35)
        return 0xffbe + (e->key() - Qt::Key_F1);   // XK_F1..
    const QString t = e->text();
    if (!t.isEmpty()) return t.at(0).unicode();      // printable
    return 0;
}

int buttonMaskFromQt(Qt::MouseButtons b) {
    int m = 0;
    if (b & Qt::LeftButton)   m |= 1;
    if (b & Qt::MiddleButton) m |= 2;
    if (b & Qt::RightButton)  m |= 4;
    return m;
}
} // namespace

RdpSurfaceWidget::RdpSurfaceWidget(QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMouseTracking(true);   // report motion even without a button held
}

void RdpSurfaceWidget::emitPointer(QMouseEvent* e) {
    if (m_viewOnly || m_frame.isNull() || width() <= 0 || height() <= 0) return;
    // Map widget pixel → framebuffer pixel (the surface scales the image).
    const int fx = int(e->position().x()) * m_frame.width() / width();
    const int fy = int(e->position().y()) * m_frame.height() / height();
    emit pointerEvent(fx, fy, buttonMaskFromQt(e->buttons()));
}

void RdpSurfaceWidget::mousePressEvent(QMouseEvent* e)   { setFocus(); emitPointer(e); }
void RdpSurfaceWidget::mouseMoveEvent(QMouseEvent* e)    { emitPointer(e); }
void RdpSurfaceWidget::mouseReleaseEvent(QMouseEvent* e) { emitPointer(e); }

void RdpSurfaceWidget::keyPressEvent(QKeyEvent* e) {
    if (m_viewOnly) return;
    const quint32 ks = qtKeyToKeysym(e);
    if (ks) emit keyEvent(ks, true);
}

void RdpSurfaceWidget::keyReleaseEvent(QKeyEvent* e) {
    if (m_viewOnly) return;
    const quint32 ks = qtKeyToKeysym(e);
    if (ks) emit keyEvent(ks, false);
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
