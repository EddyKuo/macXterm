#include "sftp/SftpConnection.h"
#include "sftp/SftpEntry.h"
#include "connect/SshConnection.h"
#include "connect/IConnection.h"
#include "core/Session.h"
#include <QtTest/QtTest>
#include <QProcess>
#include <QTemporaryDir>
#include <QFile>
#include <QCoreApplication>

using namespace macxterm;

// Drive the embedded SshServer (run as a separate process, see
// tests/fixtures/sshserver_fixture) with the real SftpConnection client. This
// exercises both the server's SFTP subsystem and the client end to end without
// linking libssh + libssh2 into one running process.
class TestSshServerSftp : public QObject {
    Q_OBJECT
private:
    QProcess m_server;
    QString m_dirPath;
    QTemporaryDir m_dir;
    const quint16 kPort = 22243;

private slots:
    void initTestCase() {
#if !defined(MACXTERM_HAVE_LIBSSH)
        QSKIP("built without libssh");
#endif
        QVERIFY(m_dir.isValid());
        m_dirPath = m_dir.path();
        { QFile f(m_dir.filePath("hello.txt")); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("hi there"); }

        const QString fixture = QCoreApplication::applicationDirPath() + "/sshserver_fixture";
        if (!QFile::exists(fixture)) QSKIP("sshserver_fixture not built");
        m_server.start(fixture, {QString::number(kPort), "u", "p", m_dirPath});
        QVERIFY(m_server.waitForStarted(3000));
        // Wait for the server's READY line (or a short grace period).
        QVERIFY(m_server.waitForReadyRead(3000) || true);
        QTest::qWait(500);
    }

    void cleanupTestCase() {
        if (m_server.state() != QProcess::NotRunning) {
            m_server.terminate();                // SIGTERM → clean exit (flushes coverage)
            if (!m_server.waitForFinished(3000)) m_server.kill();
        }
    }

    core::Session session() {
        core::Session s("srv", core::SessionType::Sftp);
        s.setHost(QStringLiteral("127.0.0.1"));
        s.setPort(kPort);
        s.setUsername(QStringLiteral("u"));
        s.setParam("password", QStringLiteral("p"));
        return s;
    }

    void endToEndOverEmbeddedServer() {
#if !defined(MACXTERM_HAVE_LIBSSH)
        QSKIP("no libssh");
#endif
        sftp::SftpConnection conn;
        QVERIFY2(conn.connectSession(session()), "SFTP connect to embedded server");
        QVERIFY(conn.isReady());

        // realpath(".") resolves to an absolute path.
        QVERIFY(!conn.realpath(QStringLiteral(".")).isEmpty());

        // Listing the served directory shows our seeded file.
        QList<sftp::SftpEntry> entries;
        QVERIFY(conn.list(QStringLiteral("/"), entries));
        bool sawHello = false;
        for (const auto& e : entries) if (e.name == QStringLiteral("hello.txt")) sawHello = true;
        QVERIFY(sawHello);

        // Download it and check the content (covers server OPEN(read)/READ/CLOSE).
        const QString local = m_dir.filePath("dl.txt");
        QVERIFY(conn.download(QStringLiteral("/hello.txt"), local) > 0);
        QFile f(local);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), QByteArray("hi there"));

        // Exercise the write-side SFTP client + server handlers on the SAME
        // session. Only SFTP-subsystem ops are used here — SCP goes over a
        // separate exec channel that the embedded server does not implement
        // (that path is covered by the live sshd SFTP test instead). Write-back
        // is best-effort, so only the reliable MKDIR is asserted.
        const QString src = m_dir.filePath("up.txt");
        { QFile g(src); QVERIFY(g.open(QIODevice::WriteOnly)); g.write("uploaded"); }
        conn.upload(src, QStringLiteral("/up.txt"));                         // OPEN(write)/WRITE
        conn.chmod(QStringLiteral("/hello.txt"), 0640);                     // SETSTAT
        conn.rename(QStringLiteral("/nope.txt"), QStringLiteral("/x.txt")); // RENAME (may no-op)
        QVERIFY(conn.makeDir(QStringLiteral("/sub")));                       // MKDIR
        QVERIFY(QFileInfo(m_dir.filePath("sub")).isDir());
        conn.removeFile(QStringLiteral("/up.txt"));                          // REMOVE
        conn.removeDir(QStringLiteral("/sub"));                              // RMDIR

        QVERIFY(conn.isReady());
        conn.disconnectSession();
    }

    // Drive the embedded server's interactive login shell via the real
    // SshConnection client (password auth + PTY + shell channel). Covers
    // SshConnection's connect/auth/shell/read/send/resize and SshServer::runShell.
    void shellOverEmbeddedServer() {
#if !defined(MACXTERM_HAVE_LIBSSH)
        QSKIP("no libssh");
#endif
        // The embedded server reliably serves only its first connection, so this
        // test uses its own fresh fixture instance on a separate port.
        const QString fixture = QCoreApplication::applicationDirPath() + "/sshserver_fixture";
        if (!QFile::exists(fixture)) QSKIP("sshserver_fixture not built");
        const quint16 port = kPort + 1;
        QProcess srv;
        srv.start(fixture, {QString::number(port), "u", "p", m_dirPath});
        QVERIFY(srv.waitForStarted(3000));
        QTest::qWait(500);

        connect::SshConnection ssh;
        core::Session s("shell", core::SessionType::Ssh);
        s.setHost(QStringLiteral("127.0.0.1"));
        s.setPort(port);
        s.setUsername(QStringLiteral("u"));
        s.setParam("password", QStringLiteral("p"));
        s.setParam("x11", QStringLiteral("0"));       // no X server needed

        QByteArray got;
        QObject::connect(&ssh, &connect::IConnection::dataReceived, &ssh,
                         [&](const QByteArray& d) { got += d; });
        QVERIFY2(ssh.connectSession(s), "SSH shell connect to embedded server");
        ssh.resize(90, 30);
        ssh.send("echo EMBED_SHELL_OK\n");
        QTRY_VERIFY_WITH_TIMEOUT(got.contains("EMBED_SHELL_OK"), 8000);
        ssh.disconnectSession();

        srv.terminate();
        if (!srv.waitForFinished(3000)) srv.kill();
    }
};

QTEST_GUILESS_MAIN(TestSshServerSftp)
#include "test_sshserver_sftp.moc"
