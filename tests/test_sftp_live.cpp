#include "sftp/SftpConnection.h"
#include "core/Session.h"
#include <QtTest/QtTest>
#include <cstdlib>

using namespace macxterm;

// Guarded-live SFTP test: connects to a real sshd (with the sftp subsystem) and
// lists a directory. Runs when MACXTERM_SSH_TEST_HOST/_USER (+ _KEY or _PASS)
// are set (see scripts/live-sshd.sh); otherwise QSKIPs.
class TestSftpLive : public QObject {
    Q_OBJECT
private slots:
    void connectsAndListsRoot() {
        const char* host = std::getenv("MACXTERM_SSH_TEST_HOST");
        const char* user = std::getenv("MACXTERM_SSH_TEST_USER");
        if (!host || !user) QSKIP("Set MACXTERM_SSH_TEST_HOST/_USER to run the live SFTP test");

        sftp::SftpConnection conn;
        core::Session s("sftp", core::SessionType::Sftp);
        s.setHost(host);
        s.setUsername(user);
        if (const char* pass = std::getenv("MACXTERM_SSH_TEST_PASS")) s.setParam("password", pass);
        if (const char* key = std::getenv("MACXTERM_SSH_TEST_KEY")) s.setParam("keyfile", key);
        if (const char* port = std::getenv("MACXTERM_SSH_TEST_PORT")) s.setPort(QString(port).toInt());

        QVERIFY(conn.connectSession(s));
        QVERIFY(conn.isReady());

        QList<sftp::SftpEntry> entries;
        QVERIFY(conn.list(QStringLiteral("."), entries));
        QVERIFY(!entries.isEmpty());          // home dir has at least "." / ".."
        conn.disconnectSession();
    }
};

QTEST_GUILESS_MAIN(TestSftpLive)
#include "test_sftp_live.moc"
