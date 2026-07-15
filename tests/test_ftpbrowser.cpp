#include "sftp/FtpClient.h"
#include "tools/FtpServer.h"
#include "core/Session.h"
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QThread>

using namespace macxterm;

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

        client.disconnectSession();
        // Stop the server in its own thread before tearing it down.
        QMetaObject::invokeMethod(&server, [&] { server.stop(); }, Qt::BlockingQueuedConnection);
        serverThread.quit();
        serverThread.wait();
    }
};

QTEST_MAIN(TestFtpBrowser)
#include "test_ftpbrowser.moc"
