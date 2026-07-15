#include "connect/RfbProtocol.h"
#include <algorithm>
#include <zlib.h>

namespace macxterm::connect::rfb {

Version parseVersion(const QByteArray& bytes) {
    Version v;
    // Expected: "RFB xxx.yyy\n" (12 bytes).
    if (bytes.size() < 12 || !bytes.startsWith("RFB ") || bytes[11] != '\n') return v;
    bool okMaj = false, okMin = false;
    const int major = bytes.mid(4, 3).toInt(&okMaj);
    const int minor = bytes.mid(8, 3).toInt(&okMin);
    if (!okMaj || !okMin) return v;
    v.major = major; v.minor = minor; v.valid = true;
    return v;
}

QByteArray formatVersion(const Version& v) {
    return QByteArray("RFB ")
        + QByteArray::number(v.major).rightJustified(3, '0')
        + "."
        + QByteArray::number(v.minor).rightJustified(3, '0')
        + "\n";
}

ServerInit parseServerInit(const QByteArray& b) {
    ServerInit s;
    if (b.size() < 24) return s;
    auto u16 = [&](int off) -> quint16 {
        return (static_cast<quint8>(b[off]) << 8) | static_cast<quint8>(b[off + 1]);
    };
    auto u32 = [&](int off) -> quint32 {
        return (static_cast<quint32>(static_cast<quint8>(b[off])) << 24)
             | (static_cast<quint32>(static_cast<quint8>(b[off + 1])) << 16)
             | (static_cast<quint32>(static_cast<quint8>(b[off + 2])) << 8)
             |  static_cast<quint32>(static_cast<quint8>(b[off + 3]));
    };
    s.width  = u16(0);
    s.height = u16(2);
    s.bitsPerPixel = static_cast<quint8>(b[4]);
    s.depth        = static_cast<quint8>(b[5]);
    const quint32 nameLen = u32(20);
    if (b.size() < static_cast<int>(24 + nameLen)) return s;
    s.name = QString::fromUtf8(b.mid(24, nameLen));
    s.valid = true;
    return s;
}

FramebufferUpdate parseFramebufferUpdate(const QByteArray& b) {
    FramebufferUpdate u;
    // message-type(1)=0, padding(1), num-rectangles(2), then rects.
    if (b.size() < 4 || static_cast<quint8>(b[0]) != 0) return u;
    auto u16 = [&](int off) -> quint16 {
        return (static_cast<quint8>(b[off]) << 8) | static_cast<quint8>(b[off + 1]);
    };
    auto s32 = [&](int off) -> qint32 {
        return (static_cast<quint32>(static_cast<quint8>(b[off])) << 24)
             | (static_cast<quint32>(static_cast<quint8>(b[off + 1])) << 16)
             | (static_cast<quint32>(static_cast<quint8>(b[off + 2])) << 8)
             |  static_cast<quint32>(static_cast<quint8>(b[off + 3]));
    };
    const int n = u16(2);
    int off = 4;
    for (int i = 0; i < n; ++i) {
        if (b.size() < off + 12) return u;   // each rect header is 12 bytes
        Rectangle r;
        r.x = u16(off); r.y = u16(off + 2);
        r.width = u16(off + 4); r.height = u16(off + 6);
        r.encoding = s32(off + 8);
        u.rects.push_back(r);
        off += 12;
    }
    u.valid = true;
    return u;
}

QList<quint32> decodeRawRect(const Rectangle& r, const QByteArray& pixelData, int bpp) {
    QList<quint32> pixels;
    const int count = int(r.width) * int(r.height);
    if (pixelData.size() < count * bpp) return pixels;
    pixels.reserve(count);
    for (int i = 0; i < count; ++i) {
        const int o = i * bpp;
        // Little-endian BGRA/BGRX in the common 32-bpp server format.
        const quint8 blue  = static_cast<quint8>(pixelData[o]);
        const quint8 green = static_cast<quint8>(pixelData[o + 1]);
        const quint8 red   = static_cast<quint8>(pixelData[o + 2]);
        pixels.push_back((0xFFu << 24) | (red << 16) | (green << 8) | blue);
    }
    return pixels;
}

// Read one server pixel as 0xAARRGGBB (little-endian BGRX, the common 32-bpp
// true-colour format). For bpp < 4 the low bytes are treated as blue/green/red.
static quint32 readPixel(const QByteArray& b, int off, int bpp) {
    const quint8 blue  = static_cast<quint8>(b[off]);
    const quint8 green = bpp > 1 ? static_cast<quint8>(b[off + 1]) : 0;
    const quint8 red   = bpp > 2 ? static_cast<quint8>(b[off + 2]) : 0;
    return (0xFFu << 24) | (red << 16) | (green << 8) | blue;
}

static quint16 rd16be(const QByteArray& b, int off) {
    return (static_cast<quint8>(b[off]) << 8) | static_cast<quint8>(b[off + 1]);
}

RectData decodeRect(const Rectangle& r, const QByteArray& buf, int off, int bpp) {
    RectData out;
    const int w = int(r.width), h = int(r.height);
    const int n = w * h;

    switch (r.encoding) {
    case EncRaw: {
        const int need = n * bpp;
        if (buf.size() < off + need) return out;
        out.pixels = decodeRawRect(r, buf.mid(off, need), bpp);
        out.consumed = need; out.complete = true;
        return out;
    }
    case EncCopyRect: {
        if (buf.size() < off + 4) return out;
        out.isCopy = true;
        out.srcX = rd16be(buf, off);
        out.srcY = rd16be(buf, off + 2);
        out.consumed = 4; out.complete = true;
        return out;
    }
    case EncRRE: {
        if (buf.size() < off + 4 + bpp) return out;
        const quint32 nsub = (static_cast<quint32>(static_cast<quint8>(buf[off])) << 24)
                           | (static_cast<quint32>(static_cast<quint8>(buf[off + 1])) << 16)
                           | (static_cast<quint32>(static_cast<quint8>(buf[off + 2])) << 8)
                           |  static_cast<quint32>(static_cast<quint8>(buf[off + 3]));
        const int subSize = bpp + 8;
        const int need = 4 + bpp + int(nsub) * subSize;
        if (buf.size() < off + need) return out;
        const quint32 bg = readPixel(buf, off + 4, bpp);
        out.pixels = QList<quint32>(n, bg);
        int p = off + 4 + bpp;
        for (quint32 s = 0; s < nsub; ++s, p += subSize) {
            const quint32 color = readPixel(buf, p, bpp);
            const int sx = rd16be(buf, p + bpp), sy = rd16be(buf, p + bpp + 2);
            const int sw = rd16be(buf, p + bpp + 4), sh = rd16be(buf, p + bpp + 6);
            for (int yy = 0; yy < sh; ++yy)
                for (int xx = 0; xx < sw; ++xx) {
                    const int px = sx + xx, py = sy + yy;
                    if (px >= 0 && px < w && py >= 0 && py < h) out.pixels[py * w + px] = color;
                }
        }
        out.consumed = need; out.complete = true;
        return out;
    }
    case EncHextile: {
        out.pixels = QList<quint32>(n, 0xFF000000u);
        quint32 bg = 0xFF000000u, fg = 0xFFFFFFFFu;
        int p = off;
        for (int ty = 0; ty < h; ty += 16) {
            const int th = std::min(16, h - ty);
            for (int tx = 0; tx < w; tx += 16) {
                const int tw = std::min(16, w - tx);
                if (buf.size() < p + 1) return out;
                const quint8 sub = static_cast<quint8>(buf[p++]);
                auto paint = [&](int lx, int ly, int lw, int lh, quint32 c) {
                    for (int yy = 0; yy < lh; ++yy)
                        for (int xx = 0; xx < lw; ++xx) {
                            const int px = tx + lx + xx, py = ty + ly + yy;
                            if (px < w && py < h) out.pixels[py * w + px] = c;
                        }
                };
                if (sub & 0x01) {   // Raw tile
                    const int need = tw * th * bpp;
                    if (buf.size() < p + need) return out;
                    for (int yy = 0; yy < th; ++yy)
                        for (int xx = 0; xx < tw; ++xx) {
                            const int px = tx + xx, py = ty + yy;
                            if (px < w && py < h)
                                out.pixels[py * w + px] = readPixel(buf, p + (yy * tw + xx) * bpp, bpp);
                        }
                    p += need;
                    continue;
                }
                if (sub & 0x02) { if (buf.size() < p + bpp) return out; bg = readPixel(buf, p, bpp); p += bpp; }
                if (sub & 0x04) { if (buf.size() < p + bpp) return out; fg = readPixel(buf, p, bpp); p += bpp; }
                paint(0, 0, tw, th, bg);   // tile background
                if (sub & 0x08) {          // AnySubrects
                    if (buf.size() < p + 1) return out;
                    const int ns = static_cast<quint8>(buf[p++]);
                    const bool coloured = sub & 0x10;
                    for (int s = 0; s < ns; ++s) {
                        quint32 c = fg;
                        if (coloured) { if (buf.size() < p + bpp) return out; c = readPixel(buf, p, bpp); p += bpp; }
                        if (buf.size() < p + 2) return out;
                        const quint8 xy = static_cast<quint8>(buf[p]);
                        const quint8 wh = static_cast<quint8>(buf[p + 1]);
                        p += 2;
                        paint(xy >> 4, xy & 0x0f, (wh >> 4) + 1, (wh & 0x0f) + 1, c);
                    }
                }
            }
        }
        out.consumed = p - off; out.complete = true;
        return out;
    }
    default:
        return out;   // unknown encoding → wait / skip
    }
}

QByteArray encodeSetEncodings(const QList<qint32>& encodings) {
    QByteArray msg;
    msg.append(char(2));                      // SetEncodings
    msg.append(char(0));                      // padding
    const int n = encodings.size();
    msg.append(char((n >> 8) & 0xff)); msg.append(char(n & 0xff));
    for (qint32 e : encodings) {
        msg.append(char((e >> 24) & 0xff)); msg.append(char((e >> 16) & 0xff));
        msg.append(char((e >> 8) & 0xff));  msg.append(char(e & 0xff));
    }
    return msg;
}

QByteArray encodePointerEvent(int x, int y, int buttonMask) {
    const int px = x < 0 ? 0 : x;
    const int py = y < 0 ? 0 : y;
    QByteArray msg;
    msg.append(char(5));                          // PointerEvent
    msg.append(char(buttonMask & 0xff));
    msg.append(char((px >> 8) & 0xff)); msg.append(char(px & 0xff));
    msg.append(char((py >> 8) & 0xff)); msg.append(char(py & 0xff));
    return msg;
}

QByteArray encodeKeyEvent(quint32 keysym, bool down) {
    QByteArray msg;
    msg.append(char(4));                          // KeyEvent
    msg.append(char(down ? 1 : 0));
    msg.append(char(0)); msg.append(char(0));     // padding
    msg.append(char((keysym >> 24) & 0xff));
    msg.append(char((keysym >> 16) & 0xff));
    msg.append(char((keysym >> 8) & 0xff));
    msg.append(char(keysym & 0xff));
    return msg;
}

// ── ZRLE (RFB 7.7.5): zlib-compressed 64×64 tiles ──

struct RfbZlibStream::Impl {
    z_stream zs {};
    bool inited = false;
};

RfbZlibStream::RfbZlibStream() : d(new Impl) {
    if (inflateInit(&d->zs) != Z_OK) m_failed = true;
    else d->inited = true;
}

RfbZlibStream::~RfbZlibStream() {
    if (d->inited) inflateEnd(&d->zs);
    delete d;
}

QByteArray RfbZlibStream::inflate(const QByteArray& compressed) {
    if (m_failed || compressed.isEmpty()) return {};
    d->zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(compressed.constData()));
    d->zs.avail_in = static_cast<uInt>(compressed.size());
    QByteArray out;
    char buf[16384];
    // Drain on the FULL-OUTPUT-BUFFER signal, not on avail_in: a completely
    // filled output buffer means zlib may still hold pending bytes (an in-flight
    // match), even once all input is consumed. Stopping at avail_in==0 could
    // strand those bytes and mis-attribute them to the next rectangle, since the
    // inflate context is persistent. Keep calling while avail_out hits 0.
    int rc;
    do {
        d->zs.next_out = reinterpret_cast<Bytef*>(buf);
        d->zs.avail_out = sizeof(buf);
        rc = ::inflate(&d->zs, Z_SYNC_FLUSH);
        if (rc != Z_OK && rc != Z_STREAM_END && rc != Z_BUF_ERROR) { m_failed = true; return out; }
        out.append(buf, static_cast<int>(sizeof(buf) - d->zs.avail_out));
        if (rc == Z_STREAM_END || rc == Z_BUF_ERROR) break;   // done / needs more input
    } while (d->zs.avail_out == 0);
    return out;
}

namespace {
// A bounds-checked cursor over inflated ZRLE tile data. `bad` latches on overrun
// so a malformed stream fails cleanly instead of reading past the buffer.
struct TileReader {
    const QByteArray& b;
    int p = 0;
    bool bad = false;
    explicit TileReader(const QByteArray& buf) : b(buf) {}

    quint8 u8() {
        if (p + 1 > b.size()) { bad = true; return 0; }
        return static_cast<quint8>(b[p++]);
    }
    // CPIXEL for 32bpp/depth-24: 3 bytes = blue, green, red (low 3 bytes).
    quint32 cpixel() {
        if (p + 3 > b.size()) { bad = true; return 0xFF000000u; }
        const quint8 blue = static_cast<quint8>(b[p]);
        const quint8 green = static_cast<quint8>(b[p + 1]);
        const quint8 red = static_cast<quint8>(b[p + 2]);
        p += 3;
        return 0xFF000000u | (red << 16) | (green << 8) | blue;
    }
    // ZRLE run length: 1 + sum of bytes, reading 255s until a byte < 255.
    int runLength() {
        int sum = 0; quint8 c;
        do { c = u8(); sum += c; } while (c == 255 && !bad);
        return sum + 1;
    }
};
}  // namespace

RectData decodeZRLERect(RfbZlibStream& z, const Rectangle& r,
                        const QByteArray& buf, int off, int /*bpp*/) {
    RectData out;
    if (buf.size() < off + 4) return out;
    const quint32 len = (static_cast<quint32>(static_cast<quint8>(buf[off])) << 24)
                      | (static_cast<quint32>(static_cast<quint8>(buf[off + 1])) << 16)
                      | (static_cast<quint32>(static_cast<quint8>(buf[off + 2])) << 8)
                      |  static_cast<quint32>(static_cast<quint8>(buf[off + 3]));
    if (buf.size() < off + 4 + static_cast<int>(len)) return out;   // wait for more

    const QByteArray tiles = z.inflate(buf.mid(off + 4, static_cast<int>(len)));
    if (z.failed()) { out.consumed = 4 + int(len); out.complete = true; return out; }  // skip cleanly

    const int w = int(r.width), h = int(r.height);
    out.pixels = QList<quint32>(w * h, 0xFF000000u);
    TileReader tr(tiles);

    for (int ty = 0; ty < h && !tr.bad; ty += 64) {
        const int th = std::min(64, h - ty);
        for (int tx = 0; tx < w && !tr.bad; tx += 64) {
            const int tw = std::min(64, w - tx);
            auto put = [&](int lx, int ly, quint32 c) {
                const int px = tx + lx, py = ty + ly;
                if (px < w && py < h) out.pixels[py * w + px] = c;
            };
            const quint8 sub = tr.u8();

            if (sub == 0) {                       // raw
                for (int y = 0; y < th; ++y)
                    for (int x = 0; x < tw; ++x) put(x, y, tr.cpixel());
            } else if (sub == 1) {                // solid colour
                const quint32 c = tr.cpixel();
                for (int y = 0; y < th; ++y)
                    for (int x = 0; x < tw; ++x) put(x, y, c);
            } else if (sub >= 2 && sub <= 16) {   // packed palette
                const int palSize = sub;
                QList<quint32> pal(palSize);
                for (int i = 0; i < palSize; ++i) pal[i] = tr.cpixel();
                const int bpp = palSize <= 2 ? 1 : (palSize <= 4 ? 2 : 4);
                const quint32 mask = (1u << bpp) - 1;
                for (int y = 0; y < th && !tr.bad; ++y) {
                    int bit = 0; quint8 cur = 0;
                    for (int x = 0; x < tw; ++x) {
                        if (bit == 0) { cur = tr.u8(); bit = 8; }
                        bit -= bpp;
                        const quint32 idx = (cur >> bit) & mask;
                        put(x, y, idx < static_cast<quint32>(palSize) ? pal[idx] : 0xFF000000u);
                    }
                    // rows are padded to a byte boundary (bit resets next row)
                }
            } else if (sub == 128) {              // plain RLE
                int x = 0, y = 0;
                while (y < th && !tr.bad) {
                    const quint32 c = tr.cpixel();
                    int run = tr.runLength();
                    while (run-- > 0 && y < th) {
                        put(x, y, c);
                        if (++x >= tw) { x = 0; ++y; }
                    }
                }
            } else if (sub >= 130) {              // palette RLE
                const int palSize = sub - 128;
                QList<quint32> pal(palSize);
                for (int i = 0; i < palSize; ++i) pal[i] = tr.cpixel();
                int x = 0, y = 0;
                while (y < th && !tr.bad) {
                    const quint8 idxByte = tr.u8();
                    const quint32 idx = idxByte & 0x7f;
                    int run = (idxByte & 0x80) ? tr.runLength() : 1;
                    const quint32 c = idx < static_cast<quint32>(palSize) ? pal[idx] : 0xFF000000u;
                    while (run-- > 0 && y < th) {
                        put(x, y, c);
                        if (++x >= tw) { x = 0; ++y; }
                    }
                }
            } else {
                tr.bad = true;                    // unused subencoding (17-127, 129)
            }
        }
    }

    if (tr.bad) { out.pixels.clear(); }           // malformed → drop, but consume
    out.consumed = 4 + int(len);
    out.complete = true;
    return out;
}

} // namespace macxterm::connect::rfb
