#include "platform/Pty.h"
#include "connect/LocalShellConnection.h"
#include <QtTest/QtTest>
#include <QTemporaryDir>

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

    // A non-empty workDir must place the child there (regression: a fresh login
    // shell otherwise inherits macXterm's cwd — "/" under a Finder launch —
    // instead of the user's home).
    void workDirPlacesChildThere() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        // Compare against the canonical path: /bin/pwd resolves symlinks (macOS
        // temp dirs live under /var -> /private/var), so we must too.
        const QString canonical = QFileInfo(tmp.path()).canonicalFilePath();
        QVERIFY(!canonical.isEmpty());

        platform::Pty pty;
        QByteArray collected;
        connect(&pty, &platform::Pty::readyRead,
                [&](const QByteArray& d) { collected += d; });
        QVERIFY(pty.start("/bin/pwd", {}, 80, 24, QString(), canonical));
        QDeadlineTimer deadline(3000);
        while (!collected.contains(canonical.toUtf8()) && !deadline.hasExpired()) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            QTest::qWait(20);
        }
        QVERIFY2(collected.contains(canonical.toUtf8()), collected.constData());
    }
};

QTEST_GUILESS_MAIN(TestPty)
#include "test_pty.moc"
