#include "core/CliOptions.h"
#include <QtTest/QtTest>

using namespace macxterm::core;

class TestCli : public QObject {
    Q_OBJECT
private slots:
    void parsesValueFlags() {
        auto o = CliOptions::parse({"-bookmark", "web1", "-exec", "top", "-i", "/cfg.ini"});
        QCOMPARE(o.bookmark, QStringLiteral("web1"));
        QCOMPARE(o.exec, QStringLiteral("top"));
        QCOMPARE(o.configPath, QStringLiteral("/cfg.ini"));
    }

    void parsesBoolFlags() {
        auto o = CliOptions::parse({"-newtab", "-noX", "-hideterm"});
        QVERIFY(o.newTab);
        QVERIFY(o.noX);
        QVERIFY(o.hideTerm);
    }

    void supportsEqualsForm() {
        auto o = CliOptions::parse({"-bookmark=db", "-runmacro=deploy"});
        QCOMPARE(o.bookmark, QStringLiteral("db"));
        QCOMPARE(o.runMacro, QStringLiteral("deploy"));
    }

    void ignoresUnknownFlags() {
        auto o = CliOptions::parse({"-unknown", "value", "-newtab"});
        QVERIFY(o.newTab);
        QVERIFY(o.exec.isEmpty());
    }

    void parsesNewFlags() {
        auto o = CliOptions::parse(
            {"-exitwhendone", "-dpi", "144", "-log", "/tmp/s.log", "-shortcuts", "/sc.ini"});
        QVERIFY(o.exitWhenDone);
        QCOMPARE(o.dpi, 144);
        QCOMPARE(o.logPath, QStringLiteral("/tmp/s.log"));
        QCOMPARE(o.shortcutsPath, QStringLiteral("/sc.ini"));
    }

    void configIsAliasOfI() {
        QCOMPARE(CliOptions::parse({"-config", "/a.ini"}).configPath, QStringLiteral("/a.ini"));
        QCOMPARE(CliOptions::parse({"-dpi=200"}).dpi, 200);
    }
};

QTEST_APPLESS_MAIN(TestCli)
#include "test_cli.moc"
