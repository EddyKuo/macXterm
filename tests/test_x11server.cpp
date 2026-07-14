#include "x11/X11Server.h"
#include <QtTest/QtTest>

using namespace macxterm::x11;

class TestX11Server : public QObject {
    Q_OBJECT
private slots:
    void candidatesAreNonEmpty() {
        const QStringList c = X11Server::candidateCommands();
        QVERIFY(!c.isEmpty());
        // Each candidate must name a program token.
        for (const QString& cmd : c) QVERIFY(!cmd.trimmed().isEmpty());
    }

    void currentDisplayFromEnv() {
        qputenv("DISPLAY", "localhost:12.0");
        QCOMPARE(X11Server::currentDisplay(), QStringLiteral("localhost:12.0"));
        qunsetenv("DISPLAY");
        // With no DISPLAY set, a sensible platform default is returned.
        QVERIFY(!X11Server::currentDisplay().isEmpty());
    }

    void isRunningReflectsSocket() {
        // Pointing DISPLAY at a definitely-absent socket path → not running.
        qputenv("DISPLAY", "/nonexistent/x11/socket");
        QVERIFY(!X11Server::isRunning());
        qunsetenv("DISPLAY");
    }
};

QTEST_APPLESS_MAIN(TestX11Server)
#include "test_x11server.moc"
