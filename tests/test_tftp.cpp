#include "tools/TftpPacket.h"
#include <QtTest/QtTest>

using namespace macxterm::tools::tftp;

class TestTftp : public QObject {
    Q_OBJECT
private slots:
    void decodeRrq() {
        QByteArray b;
        b.append('\0'); b.append(char(1));      // opcode RRQ
        b.append("boot.bin"); b.append('\0');
        b.append("octet"); b.append('\0');
        Packet p = decode(b);
        QVERIFY(p.valid);
        QCOMPARE(p.op, Op::RRQ);
        QCOMPARE(p.filename, QStringLiteral("boot.bin"));
        QCOMPARE(p.mode, QStringLiteral("octet"));
    }

    void decodeData() {
        QByteArray b;
        b.append('\0'); b.append(char(3));      // DATA
        b.append('\0'); b.append(char(5));      // block 5
        b.append("payload");
        Packet p = decode(b);
        QVERIFY(p.valid);
        QCOMPARE(p.op, Op::DATA);
        QCOMPARE(p.block, quint16(5));
        QCOMPARE(p.data, QByteArray("payload"));
    }

    void encodeAckRoundTrip() {
        QByteArray ack = encodeAck(42);
        Packet p = decode(ack);
        QCOMPARE(p.op, Op::ACK);
        QCOMPARE(p.block, quint16(42));
    }

    void encodeErrorRoundTrip() {
        QByteArray err = encodeError(1, "File not found");
        Packet p = decode(err);
        QCOMPARE(p.op, Op::ERROR);
        QCOMPARE(p.errorCode, quint16(1));
        QCOMPARE(p.errorMsg, QStringLiteral("File not found"));
    }

    void rejectsShortPacket() {
        QVERIFY(!decode(QByteArray(1, '\0')).valid);
    }
};

QTEST_APPLESS_MAIN(TestTftp)
#include "test_tftp.moc"
