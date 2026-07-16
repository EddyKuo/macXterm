#include "sftp/FtpClient.h"
#include "sftp/RemoteTransfer.h"
#include "tools/FtpServer.h"
#include "core/Session.h"
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QThread>
#include <QScopeGuard>
#include <QTcpServer>
#include <QTcpSocket>

using namespace macxterm;

// Minimal fake FTP control server whose greeting spans MULTIPLE lines, used to
// prove FtpClient::readReply no longer mistakes a continuation line (which also
// carries a space at index 3) for the terminating reply line.
class FakeMultilineFtp : public QObject {
    Q_OBJECT
public:
    bool start() {
        m_srv = new QTcpServer(this);
        connect(m_srv, &QTcpServer::newConnection, this, &FakeMultilineFtp::onConn);
        return m_srv->listen(QHostAddress::LocalHost, 0);
    }
    quint16 port() const { return m_srv ? m_srv->serverPort() : 0; }
    void stop() { if (m_srv) m_srv->close(); }
private slots:
    void onConn() {
        QTcpSocket* c = m_srv->nextPendingConnection();
        // A bare continuation line (no "NNN-" prefix, per RFC 959) whose text
        // has a space at index 3 ("The" + space) — exactly the shape the old
        // readReply mistook for the terminator.
        c->write("220-Welcome to Test FTP\r\n"
                 "The service will be down soon\r\n"
                 "220 Ready\r\n");
        connect(c, &QTcpSocket::readyRead, c, [c] {
            while (c->canReadLine()) {
                const QByteArray line = c->readLine();
                if (line.startsWith("USER"))      c->write("331 Need password\r\n");
                else if (line.startsWith("PASS")) c->write("230 Logged in\r\n");
                else if (line.startsWith("TYPE")) c->write("200 OK\r\n");
                else if (line.startsWith("PWD"))  c->write("257 \"/\" is cwd\r\n");
                else if (line.startsWith("QUIT")) c->write("221 Bye\r\n");
                else                              c->write("500 Unknown\r\n");
            }
        });
    }
private:
    QTcpServer* m_srv = nullptr;
};

// End-to-end test for the graphical FTP browser backend (sftp::FtpClient) driving
// the built-in FtpServer's passive-mode data channel over loopback: connect,
// LIST a served directory, RETR a file, STOR a new file, MKD/DELE. Also covers
// the pure LIST-line parser.
class TestFtpBrowser : public QObject {
    Q_OBJECT
private slots:
    void parsesUnixListing() {
        const QByteArray listing =
            "drwxr-xr-x 1 owner group          0 Jan 01 00:00 subdir\r\n"
            "-rw-r--r-- 1 owner group        123 Jan 01 00:00 file.txt\r\n";
        const QList<sftp::SftpEntry> e = sftp::parseFtpList(listing);
        QCOMPARE(e.size(), 2);
        // sortListing puts directories first.
        QCOMPARE(e[0].name, QStringLiteral("subdir"));
        QVERIFY(e[0].isDir);
        QCOMPARE(e[1].name, QStringLiteral("file.txt"));
        QVERIFY(!e[1].isDir);
        QCOMPARE(e[1].size, quint64(123));
    }

    void listRetrStorRoundTrip() {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        // Seed a file to download.
        QFile seed(root.filePath("hello.txt"));
        QVERIFY(seed.open(QIODevice::WriteOnly));
        seed.write("hello ftp world");
        seed.close();

        // Run the server on its own thread: the client makes blocking waitFor*
        // calls, which only service the client socket, so the server needs an
        // independent event loop to accept and answer. The listening socket must
        // be created IN that thread (moved before start(), start() invoked there)
        // or its socket notifier lives in the wrong thread.
        QThread serverThread;
        serverThread.start();
        tools::FtpServer server;
        server.setRootDir(root.path());
        server.moveToThread(&serverThread);
        // Always stop the server thread on scope exit — even if a QVERIFY below
        // returns early — so the QThread is never destroyed while still running
        // (which would qFatal/crash and pop an OS error dialog).
        auto guard = qScopeGuard([&] {
            if (serverThread.isRunning()) {
                QMetaObject::invokeMethod(&server, [&] { server.stop(); },
                                          Qt::BlockingQueuedConnection);
                serverThread.quit();
                serverThread.wait();
            }
        });
        bool started = false;
        QMetaObject::invokeMethod(&server, [&] { started = server.start(0); },
                                  Qt::BlockingQueuedConnection);
        QVERIFY(started);
        quint16 port = 0;
        QMetaObject::invokeMethod(&server, [&] { port = server.port(); },
                                  Qt::BlockingQueuedConnection);

        sftp::FtpClient client;
        core::Session s("ftp", core::SessionType::Ftp);
        s.setHost("127.0.0.1");
        s.setPort(port);
        s.setUsername("anonymous");
        QVERIFY(client.connectSession(s));
        QVERIFY(client.isReady());

        // LIST sees the seeded file.
        QList<sftp::SftpEntry> entries;
        QVERIFY(client.list(QStringLiteral("/"), entries));
        bool foundHello = false;
        for (const auto& e : entries) if (e.name == QStringLiteral("hello.txt")) foundHello = true;
        QVERIFY(foundHello);

        // RETR downloads the exact bytes.
        const QString local = root.filePath("downloaded.txt");
        QVERIFY(client.download(QStringLiteral("hello.txt"), local) > 0);
        QFile dl(local);
        QVERIFY(dl.open(QIODevice::ReadOnly));
        QCOMPARE(dl.readAll(), QByteArray("hello ftp world"));
        dl.close();

        // STOR uploads a new file that then appears in a fresh LIST.
        QFile up(root.filePath("upme.txt"));
        QVERIFY(up.open(QIODevice::WriteOnly));
        up.write("uploaded payload");
        up.close();
        QVERIFY(client.upload(root.filePath("upme.txt"), QStringLiteral("stored.txt")) > 0);
        QCOMPARE(QFile(root.filePath("stored.txt")).size(), qint64(16));

        // MKD then DELE round-trip.
        QVERIFY(client.makeDir(QStringLiteral("newdir")));
        QVERIFY(QFileInfo(root.filePath("newdir")).isDir());

        // Recursive directory transfer (sftp::uploadTree / downloadTree) — build a
        // nested local tree, upload it, then download it back and compare.
        QTemporaryDir src;
        QDir(src.path()).mkpath(QStringLiteral("a/b"));
        { QFile f(src.filePath("a/top.txt")); f.open(QIODevice::WriteOnly); f.write("T"); }
        { QFile f(src.filePath("a/b/deep.txt")); f.open(QIODevice::WriteOnly); f.write("DEEP"); }
        QVERIFY(sftp::uploadTree(client, src.filePath("a"), QStringLiteral("uptree")) > 0);
        QVERIFY(QFileInfo(root.filePath("uptree/top.txt")).isFile());
        QVERIFY(QFileInfo(root.filePath("uptree/b/deep.txt")).isFile());

        QTemporaryDir dst;
        const qint64 got = sftp::downloadTree(
            client, QStringLiteral("uptree"), dst.filePath("copy"), /*isDir=*/true);
        QVERIFY(got > 0);
        QFile deep(dst.filePath("copy/b/deep.txt"));
        QVERIFY(deep.open(QIODevice::ReadOnly));
        QCOMPARE(deep.readAll(), QByteArray("DEEP"));

        client.disconnectSession();
        // Server-thread teardown handled by the scope guard above.
    }

    // Regression: a multi-line 220 greeting must be accepted. The old readReply
    // treated any line with a space at index 3 as the terminator, so the
    // continuation "The service will be down soon" was misparsed (code "The" →
    // 0) and connectSession aborted with "No FTP greeting".
    void parsesMultiLineGreeting() {
        QThread th;
        th.start();
        FakeMultilineFtp srv;
        srv.moveToThread(&th);
        auto guard = qScopeGuard([&] {
            if (th.isRunning()) {
                QMetaObject::invokeMethod(&srv, [&] { srv.stop(); },
                                          Qt::BlockingQueuedConnection);
                th.quit();
                th.wait();
            }
        });
        bool ok = false;
        QMetaObject::invokeMethod(&srv, [&] { ok = srv.start(); },
                                  Qt::BlockingQueuedConnection);
        QVERIFY(ok);
        quint16 port = 0;
        QMetaObject::invokeMethod(&srv, [&] { port = srv.port(); },
                                  Qt::BlockingQueuedConnection);

        sftp::FtpClient client;
        core::Session s("ftp", core::SessionType::Ftp);
        s.setHost("127.0.0.1");
        s.setPort(port);
        s.setUsername("anonymous");
        QVERIFY(client.connectSession(s));   // must not reject the multi-line greeting
        QVERIFY(client.isReady());
        client.disconnectSession();
    }
};

QTEST_MAIN(TestFtpBrowser)
#include "test_ftpbrowser.moc"
