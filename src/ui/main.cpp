#include "ui/MainWindow.h"
#include <QApplication>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("macXterm"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    macxterm::ui::MainWindow w;
    w.show();
    return app.exec();
}
