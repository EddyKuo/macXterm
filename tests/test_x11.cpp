#include "x11/X11Display.h"
#include <QtTest/QtTest>

using namespace macxterm::x11;

class TestX11 : public QObject {
    Q_OBJECT
private slots:
    void parseLocalDisplay() {
        DisplaySpec s = X11Display::parse(":0");
        QVERIFY(s.valid);
        QVERIFY(s.host.isEmpty());
        QCOMPARE(s.display, 0);
        QCOMPARE(s.screen, 0);
    }

    void parseHostDisplayScreen() {
        DisplaySpec s = X11Display::parse("localhost:10.2");
        QVERIFY(s.valid);
        QCOMPARE(s.host, QStringLiteral("localhost"));
        QCOMPARE(s.display, 10);
        QCOMPARE(s.screen, 2);
    }

    void rejectsMalformed() {
        QVERIFY(!X11Display::parse("nonsense").valid);
        QVERIFY(!X11Display::parse("").valid);
    }

    void formatRoundTrip() {
        DisplaySpec s = X11Display::parse("host:5.1");
        QCOMPARE(X11Display::format(s), QStringLiteral("host:5.1"));
    }

    void forwardingDisplay() {
        QCOMPARE(X11Display::forwardingDisplay(0), QStringLiteral("localhost:10.0"));
        QCOMPARE(X11Display::forwardingDisplay(3), QStringLiteral("localhost:13.0"));
    }

    void serverAvailability() {
        QVERIFY(X11Display::serverAvailable(":0"));
        QVERIFY(!X11Display::serverAvailable(""));
    }
};

QTEST_APPLESS_MAIN(TestX11)
#include "test_x11.moc"
