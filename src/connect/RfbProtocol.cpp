#include "connect/RfbProtocol.h"

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

} // namespace macxterm::connect::rfb
