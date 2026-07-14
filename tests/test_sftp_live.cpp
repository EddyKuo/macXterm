#include "sftp/SftpConnection.h"
#include "sftp/SftpEntry.h"
#include "core/Session.h"
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
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

        // Resolve the home directory to an absolute path.
        const QString home = conn.realpath(QStringLiteral("."));
        QVERIFY(!home.isEmpty());

        // Round-trip a file: upload → list → download → verify → chmod → delete.
        // Uses a unique name under the (writable) home directory.
        const QString base = QStringLiteral("macxterm_sftp_test_%1.txt")
                                 .arg(reinterpret_cast<quintptr>(&conn), 0, 16);
        const QString remote = home + "/" + base;

        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString localUp = tmp.filePath("up.txt");
        { QFile f(localUp); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("macxterm-sftp-payload"); }

        QVERIFY(conn.upload(localUp, remote) > 0);

        const QString localDown = tmp.filePath("down.txt");
        QVERIFY(conn.download(remote, localDown) > 0);
        QFile got(localDown);
        QVERIFY(got.open(QIODevice::ReadOnly));
        QCOMPARE(got.readAll(), QByteArray("macxterm-sftp-payload"));

        // SCP round-trip of the same file (alternative backend), both directions.
        const QString scpLocal = tmp.filePath("scp.txt");
        QVERIFY(conn.scpDownload(remote, scpLocal) > 0);
        const QString scpRemote = home + "/macxterm_scp_up.txt";
        QVERIFY(conn.scpUpload(localUp, scpRemote) > 0);
        conn.removeFile(scpRemote);

        QVERIFY(conn.chmod(remote, 0600));

        // mkdir / rename / rmdir on a scratch directory.
        const QString dir = home + "/macxterm_sftp_dir";
        conn.removeDir(dir);                  // best-effort clean from a prior run
        QVERIFY(conn.makeDir(dir));
        QVERIFY(conn.rename(dir, dir + "_r"));
        QVERIFY(conn.removeDir(dir + "_r"));

        QVERIFY(conn.removeFile(remote));
        conn.disconnectSession();
    }
};

QTEST_GUILESS_MAIN(TestSftpLive)
#include "test_sftp_live.moc"
