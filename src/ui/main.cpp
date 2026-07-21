#include "ui/MainWindow.h"
#include "i18n/LanguageManager.h"
#include <QApplication>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("macXterm"));
    QApplication::setOrganizationName(QStringLiteral("macXterm"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    // Install the saved (or system-matched) UI language before any widget is
    // built, so the first frame is already localized.
    macxterm::i18n::LanguageManager::instance().applySaved();
    macxterm::ui::MainWindow w;
    w.show();
    return app.exec();
}
