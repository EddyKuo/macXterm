#pragma once
#include <QDialog>
#include <QStringList>
#include <QPixmap>

class QLabel;
class QScrollArea;

namespace macxterm::ui {

// Full-screen-capable image viewer (MobaXterm's MobaPictureViewer). Opens an
// image, fits it to the window, and steps through the other images in the same
// folder with the arrow keys.
class ImageViewerDialog : public QDialog {
    Q_OBJECT
public:
    explicit ImageViewerDialog(QWidget* parent = nullptr);
    void openImage(const QString& path);

protected:
    void keyPressEvent(QKeyEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

private:
    void showIndex(int i);
    void rescale();

    QLabel* m_label = nullptr;
    QScrollArea* m_scroll = nullptr;
    QStringList m_siblings;
    QString m_dir;
    QPixmap m_pixmap;
    int m_index = -1;
};

} // namespace macxterm::ui
