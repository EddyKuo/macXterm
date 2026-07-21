#include "ui/ImageViewerDialog.h"
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QPixmap>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>

namespace macxterm::ui {

ImageViewerDialog::ImageViewerDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Image Viewer"));
    resize(800, 600);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setAlignment(Qt::AlignCenter);
    m_scroll->setStyleSheet(QStringLiteral("background:#111"));
    m_label = new QLabel(this);
    m_label->setAlignment(Qt::AlignCenter);
    m_scroll->setWidget(m_label);
    layout->addWidget(m_scroll);
}

void ImageViewerDialog::openImage(const QString& path) {
    const QFileInfo fi(path);
    // Enumerate sibling images for arrow-key navigation.
    QStringList exts;
    for (const QByteArray& b : QImageReader::supportedImageFormats())
        exts << QStringLiteral("*.%1").arg(QString::fromLatin1(b));
    m_siblings = fi.dir().entryList(exts, QDir::Files, QDir::Name);
    m_index = m_siblings.indexOf(fi.fileName());
    m_dir = fi.absolutePath();
    if (m_index < 0) { m_siblings = {fi.fileName()}; m_index = 0; m_dir = fi.absolutePath(); }
    showIndex(m_index);
}

void ImageViewerDialog::showIndex(int i) {
    if (i < 0 || i >= m_siblings.size()) return;
    m_index = i;
    const QString path = QDir(m_dir).filePath(m_siblings[i]);
    m_pixmap.load(path);
    setWindowTitle(tr("Image Viewer — %1 (%2/%3)")
                       .arg(m_siblings[i]).arg(i + 1).arg(m_siblings.size()));
    rescale();
}

void ImageViewerDialog::rescale() {
    if (m_pixmap.isNull()) { m_label->setText(tr("Cannot load image")); return; }
    m_label->setPixmap(m_pixmap.scaled(m_scroll->viewport()->size(),
                                       Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void ImageViewerDialog::resizeEvent(QResizeEvent* e) {
    QDialog::resizeEvent(e);
    rescale();
}

void ImageViewerDialog::keyPressEvent(QKeyEvent* e) {
    switch (e->key()) {
        case Qt::Key_Right: case Qt::Key_Space:
            showIndex((m_index + 1) % qMax(1, m_siblings.size())); break;
        case Qt::Key_Left:
            showIndex((m_index - 1 + m_siblings.size()) % qMax(1, m_siblings.size())); break;
        case Qt::Key_F11:
            setWindowState(windowState() ^ Qt::WindowFullScreen); break;
        case Qt::Key_Escape:
            if (windowState() & Qt::WindowFullScreen) setWindowState(Qt::WindowNoState);
            else reject();
            break;
        default: QDialog::keyPressEvent(e);
    }
}

} // namespace macxterm::ui
