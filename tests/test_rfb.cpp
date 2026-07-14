#include "connect/RfbProtocol.h"
#include <QtTest/QtTest>

using namespace macxterm::connect::rfb;

class TestRfb : public QObject {
    Q_OBJECT
private slots:
    void parseValidVersion() {
        Version v = parseVersion(QByteArray("RFB 003.008\n"));
        QVERIFY(v.valid);
        QCOMPARE(v.major, 3);
        QCOMPARE(v.minor, 8);
    }

    void rejectsMalformedVersion() {
        QVERIFY(!parseVersion(QByteArray("HTTP 1.1\r\n")).valid);
        QVERIFY(!parseVersion(QByteArray("RFB 003.008")).valid);  // no newline / short
    }

    void versionRoundTrip() {
        Version v; v.major = 3; v.minor = 8; v.valid = true;
        QCOMPARE(formatVersion(v), QByteArray("RFB 003.008\n"));
    }

    void parseFramebufferUpdateHeader() {
        QByteArray b;
        b.append(char(0));   // message-type FramebufferUpdate
        b.append(char(0));   // padding
        b.append(char(0)); b.append(char(2));   // 2 rectangles
        auto rect = [&](int x, int y, int w, int h, int enc) {
            auto p16 = [&](int v){ b.append(char((v>>8)&0xff)); b.append(char(v&0xff)); };
            p16(x); p16(y); p16(w); p16(h);
            b.append(char((enc>>24)&0xff)); b.append(char((enc>>16)&0xff));
            b.append(char((enc>>8)&0xff)); b.append(char(enc&0xff));
        };
        rect(0, 0, 16, 16, 0);
        rect(16, 0, 8, 8, 0);
        auto u = parseFramebufferUpdate(b);
        QVERIFY(u.valid);
        QCOMPARE(u.rects.size(), 2);
        QCOMPARE(u.rects[0].width, quint16(16));
        QCOMPARE(u.rects[1].x, quint16(16));
    }

    void decodeRawPixels() {
        Rectangle r; r.width = 2; r.height = 1; r.encoding = 0;
        QByteArray px;
        // Two BGRA pixels: red (00 00 FF FF) and green (00 FF 00 FF).
        px.append(char(0x00)); px.append(char(0x00)); px.append(char(0xFF)); px.append(char(0xFF));
        px.append(char(0x00)); px.append(char(0xFF)); px.append(char(0x00)); px.append(char(0xFF));
        auto pixels = decodeRawRect(r, px, 4);
        QCOMPARE(pixels.size(), 2);
        QCOMPARE(pixels[0], quint32(0xFFFF0000));   // opaque red
        QCOMPARE(pixels[1], quint32(0xFF00FF00));   // opaque green
    }

    void parseServerInitMsg() {
        QByteArray b;
        auto put16 = [&](int v){ b.append(char((v>>8)&0xff)); b.append(char(v&0xff)); };
        auto put32 = [&](int v){ b.append(char((v>>24)&0xff)); b.append(char((v>>16)&0xff));
                                 b.append(char((v>>8)&0xff)); b.append(char(v&0xff)); };
        put16(1024);            // width
        put16(768);             // height
        b.append(char(32));     // bpp
        b.append(char(24));     // depth
        b.append(QByteArray(14, '\0'));  // rest of pixel format (16 bytes total)
        put32(7);               // name length
        b.append("desktop");    // name
        ServerInit s = parseServerInit(b);
        QVERIFY(s.valid);
        QCOMPARE(s.width, quint16(1024));
        QCOMPARE(s.height, quint16(768));
        QCOMPARE(s.bitsPerPixel, quint8(32));
        QCOMPARE(s.name, QStringLiteral("desktop"));
    }
};

QTEST_APPLESS_MAIN(TestRfb)
#include "test_rfb.moc"
