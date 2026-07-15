#include "core/TerminalConfig.h"
#include "core/Settings.h"
#include <QtTest/QtTest>

using namespace macxterm::core;

class TestTermConfig : public QObject {
    Q_OBJECT
private slots:
    // No session overrides → everything comes from the global Settings.
    void inheritsGlobalWhenNoOverride() {
        Settings g;
        g.setValue("terminal.font", "Menlo");
        g.setValue("terminal.fontSize", 13);
        g.setValue("terminal.scheme", "Dark");
        g.setValue("terminal.scrollback", 5000);
        const TermConfig c = resolveTermConfig(g, {});
        QCOMPARE(c.fontFamily, QString("Menlo"));
        QCOMPARE(c.fontSize, 13);
        QCOMPARE(c.colorScheme, QString("Dark"));
        QCOMPARE(c.scrollbackLines, 5000);
        QCOMPARE(c.backspaceCode, QByteArray("\x7f"));   // default DEL
    }

    // Each field can be overridden per session, independently.
    void sessionOverridesWin() {
        Settings g;   // defaults: Menlo/12/Dark/10000
        QVariantMap p;
        p[termkeys::font] = "JetBrains Mono";
        p[termkeys::fontSize] = 16;
        p[termkeys::scheme] = "Solarized";
        p[termkeys::scrollback] = 200;
        p[termkeys::backspace] = "ctrl-h";
        const TermConfig c = resolveTermConfig(g, p);
        QCOMPARE(c.fontFamily, QString("JetBrains Mono"));
        QCOMPARE(c.fontSize, 16);
        QCOMPARE(c.colorScheme, QString("Solarized"));
        QCOMPARE(c.scrollbackLines, 200);
        QCOMPARE(c.backspaceCode, QByteArray("\x08"));   // ^H
    }

    // Empty/blank overrides fall back rather than blanking the value out.
    void blankOverrideInherits() {
        Settings g;
        g.setValue("terminal.font", "Menlo");
        g.setValue("terminal.fontSize", 12);
        QVariantMap p;
        p[termkeys::font] = "";        // blank → inherit
        p[termkeys::fontSize] = 0;     // 0 → inherit
        p[termkeys::scrollback] = "";  // blank → inherit
        const TermConfig c = resolveTermConfig(g, p);
        QCOMPARE(c.fontFamily, QString("Menlo"));
        QCOMPARE(c.fontSize, 12);
        QCOMPARE(c.scrollbackLines, 10000);
    }

    // Scrollback 0 is a real value (disabled), not "inherit".
    void zeroScrollbackDisables() {
        Settings g;
        g.setValue("terminal.scrollback", 10000);
        QVariantMap p;
        p[termkeys::scrollback] = 0;
        const TermConfig c = resolveTermConfig(g, p);
        QCOMPARE(c.scrollbackLines, 0);
    }

    // A non-"ctrl-h" backspace value is treated as the DEL default.
    void backspaceDefaultsToDel() {
        Settings g;
        QVariantMap p;
        p[termkeys::backspace] = "del";
        QCOMPARE(resolveTermConfig(g, p).backspaceCode, QByteArray("\x7f"));
    }
};

QTEST_APPLESS_MAIN(TestTermConfig)
#include "test_termconfig.moc"
