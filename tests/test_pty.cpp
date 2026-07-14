#include "platform/Pty.h"
#include "connect/LocalShellConnection.h"
#include <QtTest/QtTest>

using namespace macxterm;

class TestPty : public QObject {
    Q_OBJECT
private slots:
    void echoProducesOutput() {
        platform::Pty pty;
        QByteArray collected;
        connect(&pty, &platform::Pty::readyRead,
                [&](const QByteArray& d) { collected += d; });
        QVERIFY(pty.start("/bin/echo", {"MACXTERM_OK"}));
        // Drive the event loop until the marker appears or we time out.
        QDeadlineTimer deadline(3000);
        while (!collected.contains("MACXTERM_OK") && !deadline.hasExpired()) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            QTest::qWait(20);
        }
        QVERIFY2(collected.contains("MACXTERM_OK"), collected.constData());
    }

    void localShellConnects() {
        connect::LocalShellConnection conn;
        core::Session s("local", core::SessionType::Shell);
        s.setParam("shell", "/bin/sh");
        QVERIFY(conn.connectSession(s));
        QCOMPARE(conn.state(), connect::IConnection::State::Connected);

        QByteArray collected;
        connect(&conn, &connect::IConnection::dataReceived,
                [&](const QByteArray& d) { collected += d; });
        conn.send("echo MACXTERM_SHELL_OK\n");
        QDeadlineTimer deadline(4000);
        while (!collected.contains("MACXTERM_SHELL_OK") && !deadline.hasExpired()) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            QTest::qWait(20);
        }
        // The echoed command and/or its output should appear on the PTY.
        QVERIFY2(collected.contains("MACXTERM_SHELL_OK"), collected.constData());
        conn.disconnectSession();
    }
};

QTEST_GUILESS_MAIN(TestPty)
#include "test_pty.moc"
