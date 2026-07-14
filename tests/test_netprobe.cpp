#include "tools/NetProbe.h"
#include "tools/HttpServer.h"
#include <QtTest/QtTest>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QCoreApplication>
#include <thread>
#include <atomic>

using namespace macxterm::tools;

class TestNetProbe : public QObject {
    Q_OBJECT
private slots:
    void tcpPingConnects() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        const quint16 port = server.serverPort();
        const auto r = NetProbe::tcpPing(QStringLiteral("127.0.0.1"), port, 2000);
        QVERIFY(r.ok);
        QVERIFY(r.ms >= 0);
    }

    void tcpPingFailsOnClosedPort() {
        // Port 1 on loopback should refuse quickly.
        const auto r = NetProbe::tcpPing(QStringLiteral("127.0.0.1"), 1, 500);
        QVERIFY(!r.ok);
    }

    void httpingHitsLocalServer() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        HttpServer http;
        QVERIFY(http.start(dir.path(), 0));
        const quint16 port = http.port();

        // The probe blocks on synchronous socket I/O, so run it off-thread while
        // the main thread pumps the event loop that drives the in-process server.
        NetProbe::Result r;
        std::atomic<bool> done{false};
        std::thread worker([&] {
            r = NetProbe::httping(QStringLiteral("http://127.0.0.1:%1/").arg(port), 3000);
            done.store(true);
        });
        while (!done.load()) { QCoreApplication::processEvents(); QThread::msleep(5); }
        worker.join();

        QVERIFY(r.ok);
        QVERIFY(r.detail.startsWith(QStringLiteral("HTTP/")));
        http.stop();
    }
};

QTEST_MAIN(TestNetProbe)
#include "test_netprobe.moc"
