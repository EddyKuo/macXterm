#include "core/ShortcutRegistry.h"
#include <QtTest/QtTest>

using namespace macxterm::core;

class TestShortcuts : public QObject {
    Q_OBJECT
private slots:
    void hasDefaults() {
        ShortcutRegistry r;
        QVERIFY(r.has("terminal.new"));
        QVERIFY(r.has("tab.next"));
        QCOMPARE(r.sequence("tab.next"), QKeySequence("Ctrl+Tab"));
    }

    void rebindSucceedsOnFreeSequence() {
        ShortcutRegistry r;
        QVERIFY(r.rebind("terminal.new", QKeySequence("Ctrl+Alt+N")));
        QCOMPARE(r.sequence("terminal.new"), QKeySequence("Ctrl+Alt+N"));
    }

    void rebindRejectsConflict() {
        ShortcutRegistry r;
        // "Ctrl+Tab" already owned by tab.next.
        QVERIFY(!r.rebind("terminal.new", QKeySequence("Ctrl+Tab")));
        QVERIFY(r.sequence("terminal.new") != QKeySequence("Ctrl+Tab"));
    }

    void actionForLookup() {
        ShortcutRegistry r;
        QCOMPARE(r.actionFor(QKeySequence("F11")), QStringLiteral("view.fullscreen"));
        QVERIFY(r.actionFor(QKeySequence("Ctrl+Alt+Z")).isEmpty());
    }
};

QTEST_APPLESS_MAIN(TestShortcuts)
#include "test_shortcuts.moc"
