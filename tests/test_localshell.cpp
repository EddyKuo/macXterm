#include "connect/LocalShellConnection.h"
#include "connect/IConnection.h"
#include "core/Session.h"
#include <QtTest/QtTest>
#include <QFileInfo>
#include <cstdlib>

using namespace macxterm;

// Spawns a real local shell over a PTY and drives it: run a command, resize, and
// confirm output comes back. Covers LocalShellConnection connect/send/resize/
// disconnect (and, via Pty, the login-shell argv path).
class TestLocalShell : public QObject {
    Q_OBJECT
private slots:
    void runsCommandAndResizes() {
        connect::LocalShellConnection conn;
        core::Session s("shell", core::SessionType::Shell);
        s.setParam("shell", QStringLiteral("/bin/sh"));   // deterministic, fast
        s.setParam("loginshell", QStringLiteral("0"));    // don't source profiles in CI

        QByteArray got;
        connect(&conn, &connect::IConnection::dataReceived, [&](const QByteArray& d){ got += d; });
        QVERIFY(conn.connectSession(s));

        conn.resize(120, 40);
        conn.send("echo LOCALSHELL_OK\n");
        QTRY_VERIFY_WITH_TIMEOUT(got.contains("LOCALSHELL_OK"), 5000);

        conn.disconnectSession();
    }

    // A fresh shell must open in the user's home ("~"), not macXterm's inherited
    // cwd (which is "/" under a Finder/`open` launch). LocalShellConnection wires
    // $HOME into the PTY's working directory.
    void opensInHomeDirectory() {
        const char* home = std::getenv("HOME");
        if (!home || !*home) QSKIP("HOME not set in this environment");
        const QString canonicalHome = QFileInfo(QString::fromLocal8Bit(home)).canonicalFilePath();
        if (canonicalHome.isEmpty()) QSKIP("HOME does not resolve");

        connect::LocalShellConnection conn;
        core::Session s("shell", core::SessionType::Shell);
        s.setParam("shell", QStringLiteral("/bin/sh"));
        s.setParam("loginshell", QStringLiteral("0"));   // no profile could `cd` for us

        QByteArray got;
        connect(&conn, &connect::IConnection::dataReceived, [&](const QByteArray& d){ got += d; });
        QVERIFY(conn.connectSession(s));
        conn.send("pwd -P\n");   // -P: physical path, matching canonicalFilePath
        QTRY_VERIFY_WITH_TIMEOUT(got.contains(canonicalHome.toUtf8()), 5000);
        conn.disconnectSession();
    }

    void reportsCapabilities() {
        connect::LocalShellConnection conn;
        // A local shell offers no SFTP/tunnel/x11 remote capabilities.
        const auto caps = conn.capabilities();
        QVERIFY(!caps.sftp);
    }
};

QTEST_MAIN(TestLocalShell)
#include "test_localshell.moc"
