#include "tools/TftpServer.h"
#include "tools/TftpPacket.h"
#include <QtTest/QtTest>
#include <QUdpSocket>
#include <QTemporaryDir>

using namespace macxterm::tools;

class TestTftpServer : public QObject {
    Q_OBJECT
private slots:
    void servesFileViaRrq() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile f(dir.filePath("boot.txt"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("TFTP_PAYLOAD_OK");
        f.close();

        TftpServer server;
        QVERIFY(server.start(dir.path(), 0));
        QVERIFY(server.isRunning());

        QUdpSocket client;
        QVERIFY(client.bind(QHostAddress::LocalHost, 0));
        // Build RRQ: opcode 1, "boot.txt"\0"octet"\0
        QByteArray rrq;
        rrq.append('\0'); rrq.append(char(1));
        rrq.append("boot.txt"); rrq.append('\0');
        rrq.append("octet"); rrq.append('\0');
        client.writeDatagram(rrq, QHostAddress::LocalHost, server.port());

        // Await the DATA block.
        QByteArray dg;
        QTRY_VERIFY_WITH_TIMEOUT(client.hasPendingDatagrams(), 3000);
        dg.resize(int(client.pendingDatagramSize()));
        client.readDatagram(dg.data(), dg.size());
        const tftp::Packet p = tftp::decode(dg);
        QCOMPARE(p.op, tftp::Op::DATA);
        QCOMPARE(p.block, quint16(1));
        QCOMPARE(p.data, QByteArray("TFTP_PAYLOAD_OK"));
    }

    void missingFileGivesError() {
        QTemporaryDir dir;
        TftpServer server;
        QVERIFY(server.start(dir.path(), 0));
        QUdpSocket client;
        QVERIFY(client.bind(QHostAddress::LocalHost, 0));
        QByteArray rrq;
        rrq.append('\0'); rrq.append(char(1));
        rrq.append("nope.txt"); rrq.append('\0');
        rrq.append("octet"); rrq.append('\0');
        client.writeDatagram(rrq, QHostAddress::LocalHost, server.port());
        QByteArray dg;
        QTRY_VERIFY_WITH_TIMEOUT(client.hasPendingDatagrams(), 3000);
        dg.resize(int(client.pendingDatagramSize()));
        client.readDatagram(dg.data(), dg.size());
        QCOMPARE(tftp::decode(dg).op, tftp::Op::ERROR);
    }
};

QTEST_GUILESS_MAIN(TestTftpServer)
#include "test_tftpserver.moc"
