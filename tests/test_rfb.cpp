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

    void encodePointerEventWire() {
        // Left button down at (300, 200): type 5, mask 1, x=0x012C, y=0x00C8.
        const QByteArray m = encodePointerEvent(300, 200, 1);
        QByteArray want; want.append(char(5)); want.append(char(1));
        want.append(char(0x01)); want.append(char(0x2C));   // x = 300 big-endian
        want.append(char(0x00)); want.append(char(0xC8));   // y = 200 big-endian
        QCOMPARE(m, want);
    }

    void encodePointerEventClampsNegative() {
        const QByteArray m = encodePointerEvent(-5, -1, 0);
        QByteArray want; want.append(char(5)); want.append(char(0));
        want.append(char(0)); want.append(char(0));
        want.append(char(0)); want.append(char(0));
        QCOMPARE(m, want);
    }

    void encodeKeyEventWire() {
        // 'A' (0x41) key down: type 4, down 1, pad pad, keysym 00 00 00 41.
        const QByteArray m = encodeKeyEvent(0x41, true);
        QByteArray want; want.append(char(4)); want.append(char(1));
        want.append(char(0)); want.append(char(0));
        want.append(char(0)); want.append(char(0)); want.append(char(0)); want.append(char(0x41));
        QCOMPARE(m, want);
        // Key up flips only the down flag.
        const QByteArray up = encodeKeyEvent(0x41, false);
        QCOMPARE(static_cast<unsigned char>(up[1]), static_cast<unsigned char>(0));
    }

    void encodeKeyEventBigEndianKeysym() {
        // Return keysym 0xFF0D → bytes 00 00 FF 0D.
        const QByteArray m = encodeKeyEvent(0xFF0D, true);
        QCOMPARE(static_cast<unsigned char>(m[4]), static_cast<unsigned char>(0x00));
        QCOMPARE(static_cast<unsigned char>(m[5]), static_cast<unsigned char>(0x00));
        QCOMPARE(static_cast<unsigned char>(m[6]), static_cast<unsigned char>(0xFF));
        QCOMPARE(static_cast<unsigned char>(m[7]), static_cast<unsigned char>(0x0D));
    }

    // ── Encoding dispatch: CopyRect / RRE / Hextile via decodeRect ──

    void decodeRectRawViaDispatch() {
        Rectangle r; r.width = 2; r.height = 1; r.encoding = EncRaw;
        QByteArray b;   // two BGRA pixels: red, green
        b.append(char(0x00)); b.append(char(0x00)); b.append(char(0xFF)); b.append(char(0xFF));
        b.append(char(0x00)); b.append(char(0xFF)); b.append(char(0x00)); b.append(char(0xFF));
        const RectData d = decodeRect(r, b, 0, 4);
        QVERIFY(d.complete);
        QCOMPARE(d.consumed, 8);
        QCOMPARE(d.pixels.size(), 2);
        QCOMPARE(d.pixels[0], quint32(0xFFFF0000));
    }

    void decodeRectCopyRect() {
        Rectangle r; r.x = 10; r.y = 20; r.width = 4; r.height = 4; r.encoding = EncCopyRect;
        QByteArray b;   // src (x=5, y=7) big-endian u16
        b.append(char(0x00)); b.append(char(0x05));
        b.append(char(0x00)); b.append(char(0x07));
        const RectData d = decodeRect(r, b, 0, 4);
        QVERIFY(d.complete);
        QVERIFY(d.isCopy);
        QCOMPARE(d.consumed, 4);
        QCOMPARE(d.srcX, 5);
        QCOMPARE(d.srcY, 7);
    }

    void decodeRectRRE() {
        // 2×2 rect, blue background, one 1×1 red subrect at (1,0).
        Rectangle r; r.width = 2; r.height = 2; r.encoding = EncRRE;
        QByteArray b;
        b.append(char(0)); b.append(char(0)); b.append(char(0)); b.append(char(1));   // 1 subrect
        b.append(char(0xFF)); b.append(char(0x00)); b.append(char(0x00)); b.append(char(0xFF)); // bg blue
        b.append(char(0x00)); b.append(char(0x00)); b.append(char(0xFF)); b.append(char(0xFF)); // subrect red
        auto p16 = [&](int v){ b.append(char((v>>8)&0xff)); b.append(char(v&0xff)); };
        p16(1); p16(0); p16(1); p16(1);   // x=1,y=0,w=1,h=1
        const RectData d = decodeRect(r, b, 0, 4);
        QVERIFY(d.complete);
        QCOMPARE(d.consumed, 4 + 4 + (4 + 8));
        QCOMPARE(d.pixels.size(), 4);
        QCOMPARE(d.pixels[0], quint32(0xFF0000FF));   // blue bg
        QCOMPARE(d.pixels[1], quint32(0xFFFF0000));   // red subrect at (1,0)
        QCOMPARE(d.pixels[2], quint32(0xFF0000FF));
    }

    void decodeRectRREIncompleteWaits() {
        Rectangle r; r.width = 2; r.height = 2; r.encoding = EncRRE;
        QByteArray b;   // claims 1 subrect but supplies no subrect bytes
        b.append(char(0)); b.append(char(0)); b.append(char(0)); b.append(char(1));
        b.append(char(0xFF)); b.append(char(0x00)); b.append(char(0x00)); b.append(char(0xFF));
        const RectData d = decodeRect(r, b, 0, 4);
        QVERIFY(!d.complete);           // not enough bytes → wait
        QCOMPARE(d.consumed, 0);
    }

    void decodeRectHextileSolidBackground() {
        // 2×2 rect, single tile, BackgroundSpecified only → whole tile = bg green.
        Rectangle r; r.width = 2; r.height = 2; r.encoding = EncHextile;
        QByteArray b;
        b.append(char(0x02));   // subencoding: BackgroundSpecified
        b.append(char(0x00)); b.append(char(0xFF)); b.append(char(0x00)); b.append(char(0xFF)); // green
        const RectData d = decodeRect(r, b, 0, 4);
        QVERIFY(d.complete);
        QCOMPARE(d.consumed, 5);
        QCOMPARE(d.pixels.size(), 4);
        for (quint32 p : d.pixels) QCOMPARE(p, quint32(0xFF00FF00));
    }

    void decodeRectHextileForegroundSubrect() {
        // 2×2 rect: bg green, one 1×1 red foreground subrect at tile-pos (0,0).
        Rectangle r; r.width = 2; r.height = 2; r.encoding = EncHextile;
        QByteArray b;
        b.append(char(0x02 | 0x04 | 0x08));   // Background + Foreground + AnySubrects
        b.append(char(0x00)); b.append(char(0xFF)); b.append(char(0x00)); b.append(char(0xFF)); // bg green
        b.append(char(0x00)); b.append(char(0x00)); b.append(char(0xFF)); b.append(char(0xFF)); // fg red
        b.append(char(1));           // 1 subrect
        b.append(char(0x00));        // x=0,y=0
        b.append(char(0x00));        // w-1=0,h-1=0 → 1×1
        const RectData d = decodeRect(r, b, 0, 4);
        QVERIFY(d.complete);
        QCOMPARE(d.pixels.size(), 4);
        QCOMPARE(d.pixels[0], quint32(0xFFFF0000));   // red subrect at (0,0)
        QCOMPARE(d.pixels[3], quint32(0xFF00FF00));   // green bg elsewhere
    }

    void decodeRectUnknownEncodingIncomplete() {
        Rectangle r; r.width = 1; r.height = 1; r.encoding = 999;
        QVERIFY(!decodeRect(r, QByteArray(64, '\0'), 0, 4).complete);
    }

    void setEncodingsWire() {
        const QByteArray m = encodeSetEncodings({EncHextile, EncRaw});
        QByteArray want;
        want.append(char(2)); want.append(char(0));         // type, padding
        want.append(char(0)); want.append(char(2));         // count = 2
        want.append(char(0)); want.append(char(0)); want.append(char(0)); want.append(char(5)); // Hextile
        want.append(char(0)); want.append(char(0)); want.append(char(0)); want.append(char(0)); // Raw
        QCOMPARE(m, want);
    }
};

QTEST_APPLESS_MAIN(TestRfb)
#include "test_rfb.moc"
