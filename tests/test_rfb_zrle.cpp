#include "connect/RfbProtocol.h"
#include <QtTest/QtTest>
#include <zlib.h>

using namespace macxterm::connect::rfb;

namespace {

// CPIXEL for our 32bpp/depth-24 format: 3 bytes = blue, green, red (the low
// three bytes of the 0xAARRGGBB pixel).
void appendCPixel(QByteArray& b, quint32 argb) {
    b.append(char(argb & 0xff));           // blue
    b.append(char((argb >> 8) & 0xff));    // green
    b.append(char((argb >> 16) & 0xff));   // red
}

// A persistent zlib deflate stream mirroring the RFB server side: each call
// flushes (Z_SYNC_FLUSH) at the rectangle boundary, exactly as ZRLE requires.
struct Deflater {
    z_stream zs {};
    Deflater() { deflateInit(&zs, Z_DEFAULT_COMPRESSION); }
    ~Deflater() { deflateEnd(&zs); }
    QByteArray operator()(const QByteArray& in) {
        zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(in.constData()));
        zs.avail_in = static_cast<uInt>(in.size());
        QByteArray out; char buf[16384];
        do {
            zs.next_out = reinterpret_cast<Bytef*>(buf);
            zs.avail_out = sizeof(buf);
            deflate(&zs, Z_SYNC_FLUSH);
            out.append(buf, static_cast<int>(sizeof(buf) - zs.avail_out));
        } while (zs.avail_out == 0);
        return out;
    }
};

// Wrap compressed bytes in a ZRLE rectangle payload: CARD32 length + data.
QByteArray zrlePayload(const QByteArray& compressed) {
    QByteArray p;
    const quint32 n = compressed.size();
    p.append(char((n >> 24) & 0xff)); p.append(char((n >> 16) & 0xff));
    p.append(char((n >> 8) & 0xff));  p.append(char(n & 0xff));
    p.append(compressed);
    return p;
}

constexpr quint32 RED   = 0xFFFF0000u;
constexpr quint32 GREEN = 0xFF00FF00u;
constexpr quint32 BLUE  = 0xFF0000FFu;

}  // namespace

class TestRfbZrle : public QObject {
    Q_OBJECT
private slots:
    void solidTile() {
        QByteArray tile; tile.append(char(1)); appendCPixel(tile, RED);   // subencoding 1 = solid
        Deflater def; RfbZlibStream z;
        Rectangle r; r.width = 3; r.height = 3; r.encoding = EncZRLE;
        const RectData d = decodeZRLERect(z, r, zrlePayload(def(tile)), 0, 4);
        QVERIFY(d.complete);
        QCOMPARE(d.pixels.size(), 9);
        for (quint32 p : d.pixels) QCOMPARE(p, RED);
    }

    void rawTile() {
        QByteArray tile; tile.append(char(0));   // subencoding 0 = raw
        appendCPixel(tile, RED); appendCPixel(tile, GREEN);
        Deflater def; RfbZlibStream z;
        Rectangle r; r.width = 2; r.height = 1; r.encoding = EncZRLE;
        const RectData d = decodeZRLERect(z, r, zrlePayload(def(tile)), 0, 4);
        QVERIFY(d.complete);
        QCOMPARE(d.pixels.size(), 2);
        QCOMPARE(d.pixels[0], RED);
        QCOMPARE(d.pixels[1], GREEN);
    }

    void packedPalette() {
        // 4×1, 2-colour palette (blue,red), 1 bit/index; indices 0,1,0,1.
        QByteArray tile; tile.append(char(2));   // palette size 2 → packed
        appendCPixel(tile, BLUE); appendCPixel(tile, RED);
        tile.append(char(0x50));                 // 0b0101_0000 MSB-first: 0,1,0,1
        Deflater def; RfbZlibStream z;
        Rectangle r; r.width = 4; r.height = 1; r.encoding = EncZRLE;
        const RectData d = decodeZRLERect(z, r, zrlePayload(def(tile)), 0, 4);
        QVERIFY(d.complete);
        QCOMPARE(d.pixels, (QList<quint32>{BLUE, RED, BLUE, RED}));
    }

    void plainRle() {
        // 4×1 all green via one RLE run of length 4 (sum byte 3 → len 3+1).
        QByteArray tile; tile.append(char(128));   // plain RLE
        appendCPixel(tile, GREEN); tile.append(char(3));
        Deflater def; RfbZlibStream z;
        Rectangle r; r.width = 4; r.height = 1; r.encoding = EncZRLE;
        const RectData d = decodeZRLERect(z, r, zrlePayload(def(tile)), 0, 4);
        QVERIFY(d.complete);
        QCOMPARE(d.pixels, (QList<quint32>{GREEN, GREEN, GREEN, GREEN}));
    }

    void paletteRle() {
        // 4×1: palette (blue,red); run idx0×2 then idx1×2.
        QByteArray tile; tile.append(char(130));   // palette RLE, size 2
        appendCPixel(tile, BLUE); appendCPixel(tile, RED);
        tile.append(char(0x80)); tile.append(char(1));   // idx0, run 2
        tile.append(char(0x81)); tile.append(char(1));   // idx1, run 2
        Deflater def; RfbZlibStream z;
        Rectangle r; r.width = 4; r.height = 1; r.encoding = EncZRLE;
        const RectData d = decodeZRLERect(z, r, zrlePayload(def(tile)), 0, 4);
        QVERIFY(d.complete);
        QCOMPARE(d.pixels, (QList<quint32>{BLUE, BLUE, RED, RED}));
    }

    void multiTileTiling() {
        // 65×1 forces two horizontal tiles (64 + 1), each a solid colour.
        QByteArray tile;
        tile.append(char(1)); appendCPixel(tile, RED);    // tile 0 (64 wide)
        tile.append(char(1)); appendCPixel(tile, RED);    // tile 1 (1 wide)
        Deflater def; RfbZlibStream z;
        Rectangle r; r.width = 65; r.height = 1; r.encoding = EncZRLE;
        const RectData d = decodeZRLERect(z, r, zrlePayload(def(tile)), 0, 4);
        QVERIFY(d.complete);
        QCOMPARE(d.pixels.size(), 65);
        for (quint32 p : d.pixels) QCOMPARE(p, RED);
    }

    void continuousStreamAcrossRects() {
        // Two rectangles must share ONE inflate context (the compressed stream
        // is continuous). Decode with a single RfbZlibStream and verify both.
        Deflater def; RfbZlibStream z;
        QByteArray t1; t1.append(char(1)); appendCPixel(t1, RED);
        QByteArray t2; t2.append(char(1)); appendCPixel(t2, GREEN);
        const QByteArray p1 = zrlePayload(def(t1));
        const QByteArray p2 = zrlePayload(def(t2));
        Rectangle r; r.width = 2; r.height = 1; r.encoding = EncZRLE;
        const RectData d1 = decodeZRLERect(z, r, p1, 0, 4);
        QVERIFY(d1.complete);
        QCOMPARE(d1.pixels[0], RED);
        const RectData d2 = decodeZRLERect(z, r, p2, 0, 4);
        QVERIFY(d2.complete);
        QCOMPARE(d2.pixels[0], GREEN);
    }

    void incompleteBlockWaits() {
        // Buffer holds the length prefix but not the whole compressed block.
        QByteArray tile; tile.append(char(1)); appendCPixel(tile, RED);
        Deflater def; RfbZlibStream z;
        const QByteArray full = zrlePayload(def(tile));
        Rectangle r; r.width = 2; r.height = 1; r.encoding = EncZRLE;
        const RectData d = decodeZRLERect(z, r, full.left(full.size() - 2), 0, 4);
        QVERIFY(!d.complete);
        QCOMPARE(d.consumed, 0);
    }
};

QTEST_APPLESS_MAIN(TestRfbZrle)
#include "test_rfb_zrle.moc"
