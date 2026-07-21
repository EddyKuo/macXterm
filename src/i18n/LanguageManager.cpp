#include "i18n/LanguageManager.h"

#include <QCoreApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QTranslator>

namespace macxterm::i18n {

LanguageManager& LanguageManager::instance() {
    static LanguageManager mgr;
    return mgr;
}

QVector<LanguageInfo> LanguageManager::available() const {
    return {
        {QStringLiteral("en"),    QStringLiteral("English")},
        {QStringLiteral("zh_TW"), QStringLiteral("繁體中文")},
        {QStringLiteral("zh_CN"), QStringLiteral("简体中文")},
        {QStringLiteral("ja"),    QStringLiteral("日本語")},
    };
}

QString LanguageManager::nativeName(const QString& code) const {
    for (const LanguageInfo& l : available())
        if (l.code == code) return l.nativeName;
    return code;
}

void LanguageManager::applySaved() {
    QSettings qs(QStringLiteral("macXterm"), QStringLiteral("macXterm"));
    QString code = qs.value(QStringLiteral("language")).toString();
    if (code.isEmpty()) {
        // First run: honour the system UI language when we ship it.
        const QString sys = QLocale::system().name();          // e.g. "zh_TW", "ja_JP"
        for (const LanguageInfo& l : available()) {
            if (sys == l.code || sys.startsWith(l.code + QLatin1Char('_'))
                || (l.code == QStringLiteral("ja") && sys.startsWith(QStringLiteral("ja")))) {
                code = l.code;
                break;
            }
        }
        if (code.isEmpty()) code = QStringLiteral("en");
    }
    install(code);
    m_current = code;
}

void LanguageManager::setLanguage(const QString& code) {
    if (code == m_current) return;
    install(code);
    m_current = code;
    QSettings qs(QStringLiteral("macXterm"), QStringLiteral("macXterm"));
    qs.setValue(QStringLiteral("language"), code);
    emit languageChanged(code);
}

void LanguageManager::install(const QString& code) {
    auto* appInst = QCoreApplication::instance();
    if (!appInst) return;

    // Tear down whatever is installed; removeTranslator() posts LanguageChange.
    if (m_app) { QCoreApplication::removeTranslator(m_app); delete m_app; m_app = nullptr; }
    if (m_qt)  { QCoreApplication::removeTranslator(m_qt);  delete m_qt;  m_qt  = nullptr; }

    // English is the source language: no translator needed, plain source strings.
    if (code == QStringLiteral("en")) return;

    // App strings — compiled into the binary as a Qt resource by qt_add_translations.
    auto* app = new QTranslator(appInst);
    if (app->load(QStringLiteral(":/i18n/macxterm_") + code))
        QCoreApplication::installTranslator(app);
    m_app = app;

    // Qt's own strings (standard button captions, line-edit context menu, …).
    auto* qt = new QTranslator(appInst);
    const QString qtDir = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    if (qt->load(QStringLiteral("qtbase_") + code, qtDir))
        QCoreApplication::installTranslator(qt);
    m_qt = qt;
}

} // namespace macxterm::i18n
