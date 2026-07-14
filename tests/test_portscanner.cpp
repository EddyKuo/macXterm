#include "tools/PortScanner.h"
#include <QtTest/QtTest>
#include <QTcpServer>

using namespace macxterm::tools;

class TestPortScanner : public QObject {
    Q_OBJECT
private slots:
    void detectsOpenPort() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        const quint16 port = server.serverPort();
        QVERIFY(PortScanner::scanPort("127.0.0.1", port, 1000));
    }

    void detectsClosedPort() {
        // Bind then close to obtain a very-likely-free port number.
        QTcpServer probe;
        QVERIFY(probe.listen(QHostAddress::LocalHost));
        const quint16 port = probe.serverPort();
        probe.close();
        QVERIFY(!PortScanner::scanPort("127.0.0.1", port, 300));
    }

    void rangeScanEmitsOpen() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        const quint16 port = server.serverPort();

        PortScanner scanner;
        QSignalSpy openSpy(&scanner, &PortScanner::portOpen);
        QSignalSpy doneSpy(&scanner, &PortScanner::finished);
        scanner.scanRange("127.0.0.1", port, port, 500);
        QCOMPARE(openSpy.count(), 1);
        QCOMPARE(openSpy.at(0).at(0).toInt(), int(port));
        QCOMPARE(doneSpy.count(), 1);
        QCOMPARE(doneSpy.at(0).at(0).toInt(), 1);
    }
};

QTEST_GUILESS_MAIN(TestPortScanner)
#include "test_portscanner.moc"
