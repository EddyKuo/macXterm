#pragma once
#include <QObject>
#include <QString>
#include <QVector>

class QTranslator;

namespace macxterm::i18n {

// One selectable UI language: the .qm locale code (e.g. "zh_TW") and the name
// shown to the user in that language's own script ("繁體中文").
struct LanguageInfo {
    QString code;        // "en" | "zh_TW" | "zh_CN" | "ja"
    QString nativeName;  // endonym, shown verbatim in the language picker
};

// App-wide runtime language switcher. Installs the compiled .qm bundled in the
// Qt resource (:/i18n/macxterm_<code>.qm) plus Qt's own base translation for the
// standard dialog buttons, then relies on Qt posting QEvent::LanguageChange to
// every live widget so open windows retranslate without a restart. The choice is
// persisted in QSettings and re-applied on next launch.
//
// Dialogs in this app are constructed on demand, so they render in the current
// language the moment they are opened; only always-present chrome (the main
// window's menus/toolbar/docks) needs to react to LanguageChange live.
class LanguageManager : public QObject {
    Q_OBJECT
public:
    static LanguageManager& instance();

    // Every language the app ships, in menu order (English first).
    QVector<LanguageInfo> available() const;

    // Currently active locale code.
    QString current() const { return m_current; }

    // Endonym for a code (falls back to the code itself if unknown).
    QString nativeName(const QString& code) const;

    // Load the language saved in QSettings (defaults to the system UI language
    // when it is one we ship, otherwise English). Call once at startup BEFORE
    // constructing the main window.
    void applySaved();

    // Switch language now: swaps the installed translators, persists the choice,
    // and lets Qt broadcast LanguageChange to all widgets. No-op if unchanged.
    void setLanguage(const QString& code);

signals:
    void languageChanged(const QString& code);

private:
    LanguageManager() = default;
    void install(const QString& code);   // (re)install translators for `code`

    QTranslator* m_app = nullptr;   // macxterm_<code>.qm
    QTranslator* m_qt  = nullptr;   // qtbase_<code>.qm (Ok/Cancel/… in native dialogs)
    QString m_current = QStringLiteral("en");
};

} // namespace macxterm::i18n
