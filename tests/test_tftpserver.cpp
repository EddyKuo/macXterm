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

    // Regression: a file whose size is an exact multiple of the 512-byte block
    // must be terminated with a final 0-byte DATA block, or an RFC-1350 client
    // waits for it forever. Previously the server erased the transfer after the
    // full last block and sent nothing more.
    void exactBlockMultipleSendsFinalEmptyBlock() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile f(dir.filePath("exact.bin"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(512, 'A'));   // exactly one full block
        f.close();

        TftpServer server;
        QVERIFY(server.start(dir.path(), 0));
        QUdpSocket client;
        QVERIFY(client.bind(QHostAddress::LocalHost, 0));

        QByteArray rrq;
        rrq.append('\0'); rrq.append(char(1));
        rrq.append("exact.bin"); rrq.append('\0');
        rrq.append("octet"); rrq.append('\0');
        client.writeDatagram(rrq, QHostAddress::LocalHost, server.port());

        auto recv = [&]() -> tftp::Packet {
            QByteArray dg;
            [&]{ QTRY_VERIFY_WITH_TIMEOUT(client.hasPendingDatagrams(), 3000); }();
            dg.resize(int(client.pendingDatagramSize()));
            client.readDatagram(dg.data(), dg.size());
            return tftp::decode(dg);
        };
        auto ack = [&](quint16 block) {
            QByteArray a;
            a.append('\0'); a.append(char(4));
            a.append(char(block >> 8)); a.append(char(block & 0xFF));
            client.writeDatagram(a, QHostAddress::LocalHost, server.port());
        };

        // Block 1: a full 512-byte block.
        tftp::Packet p1 = recv();
        QCOMPARE(p1.op, tftp::Op::DATA);
        QCOMPARE(p1.block, quint16(1));
        QCOMPARE(p1.data.size(), 512);
        ack(1);

        // Block 2 MUST arrive and be empty — that is the transfer terminator.
        tftp::Packet p2 = recv();
        QCOMPARE(p2.op, tftp::Op::DATA);
        QCOMPARE(p2.block, quint16(2));
        QVERIFY(p2.data.isEmpty());
        ack(2);
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
