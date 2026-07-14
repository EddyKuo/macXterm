#include "tools/FtpServer.h"
#include <QtTest/QtTest>
#include <QTcpSocket>

using namespace macxterm::tools;

class TestFtpServer : public QObject {
    Q_OBJECT
private slots:
    void controlDialogOverLoopback() {
        FtpServer server;
        QVERIFY(server.start(0));
        QVERIFY(server.isRunning());

        QTcpSocket c;
        QByteArray resp;
        connect(&c, &QTcpSocket::readyRead, [&] { resp += c.readAll(); });
        c.connectToHost(QHostAddress::LocalHost, server.port());
        QVERIFY(c.waitForConnected(2000));

        // Server greets with 220.
        QTRY_VERIFY_WITH_TIMEOUT(resp.contains("220"), 3000);

        resp.clear();
        c.write("USER anonymous\r\n");
        QTRY_VERIFY_WITH_TIMEOUT(resp.contains("331"), 3000);

        resp.clear();
        c.write("PASS x@y\r\n");
        QTRY_VERIFY_WITH_TIMEOUT(resp.contains("230"), 3000);

        resp.clear();
        c.write("PWD\r\n");
        QTRY_VERIFY_WITH_TIMEOUT(resp.contains("257"), 3000);

        resp.clear();
        c.write("BOGUS\r\n");
        QTRY_VERIFY_WITH_TIMEOUT(resp.contains("502"), 3000);
    }
};

QTEST_GUILESS_MAIN(TestFtpServer)
#include "test_ftpserver.moc"
