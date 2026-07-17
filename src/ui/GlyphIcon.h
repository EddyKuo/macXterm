#pragma once

#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QGuiApplication>
#include <QFont>
#include <QRectF>
#include <QString>

namespace macxterm::ui {

// Render an emoji glyph into a crisp, menu-sized QIcon so menus and toolbars
// across the app speak the same visual language as the session tree (whose
// rows are prefixed with core::sessionGlyph). Pure presentation — returns a
// null icon for an empty glyph so callers can pass through unconditionally.
inline QIcon glyphIcon(const QString& glyph) {
    if (glyph.isEmpty()) return QIcon();
    const int px = 16;
    const qreal dpr = qGuiApp ? qGuiApp->devicePixelRatio() : 1.0;
    QPixmap pm(QSize(px, px) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    QFont f = p.font();
    f.setPointSizeF(px * 0.72);
    p.setFont(f);
    p.drawText(QRectF(0, 0, px, px), Qt::AlignCenter, glyph);
    p.end();
    return QIcon(pm);
}

}  // namespace macxterm::ui
