#pragma once
#include <QByteArray>
#include <QString>
#include <QList>

namespace macxterm::connect {

// Minimal RFB (VNC) protocol codec — MIT self-implementation, since libvncclient
// is GPL and excluded (ADR_6). Covers the pieces needed to bring up a VNC
// session: ProtocolVersion parse/format and ServerInit parse. Encodings and the
// full framebuffer pipeline come in later phases.
namespace rfb {

struct Version { int major = 0; int minor = 0; bool valid = false; };

// Parse a 12-byte "RFB 003.008\n" ProtocolVersion string.
Version parseVersion(const QByteArray& bytes);
// Format a Version back into the 12-byte wire form.
QByteArray formatVersion(const Version& v);

struct ServerInit {
    quint16 width = 0;
    quint16 height = 0;
    quint8  bitsPerPixel = 0;
    quint8  depth = 0;
    QString name;
    bool valid = false;
};

// Parse a ServerInit message: width(2) height(2) pixelFormat(16) nameLen(4) name.
ServerInit parseServerInit(const QByteArray& bytes);

// One rectangle header from a FramebufferUpdate.
struct Rectangle {
    quint16 x = 0, y = 0, width = 0, height = 0;
    qint32  encoding = 0;   // 0 = Raw
};

struct FramebufferUpdate {
    QList<Rectangle> rects;
    bool valid = false;
};

// Parse a FramebufferUpdate message header + rectangle headers (message-type 0,
// padding, rect-count, then each rect's x/y/w/h/encoding). Pixel payload is left
// to decodeRawRect(). Returns invalid if the buffer is too short.
FramebufferUpdate parseFramebufferUpdate(const QByteArray& bytes);

// Decode a RAW-encoded rectangle's pixels (bytesPerPixel each) into 0xAARRGGBB
// words, row-major. `pixelData` must hold w*h*bytesPerPixel bytes. Assumes a
// little-endian 32-bpp true-colour server format (the common case).
QList<quint32> decodeRawRect(const Rectangle& r, const QByteArray& pixelData,
                             int bytesPerPixel = 4);

// RFB rectangle encoding numbers we decode.
enum Encoding : qint32 { EncRaw = 0, EncCopyRect = 1, EncRRE = 2, EncHextile = 5, EncZRLE = 16 };

// Result of decoding one rectangle's payload out of a byte stream.
struct RectData {
    bool complete = false;        // false = the buffer doesn't yet hold the whole rect
    int  consumed = 0;            // payload bytes used (excludes the 12-byte rect header)
    QList<quint32> pixels;        // decoded ARGB, row-major (empty for CopyRect)
    bool isCopy = false;          // true → blit from (srcX,srcY) rather than `pixels`
    int  srcX = 0, srcY = 0;      // CopyRect source origin
};

// Decode one rectangle's payload (Raw / CopyRect / RRE / Hextile) beginning at
// byte `off` in `buf` (which points just past the rect's 12-byte header). `bpp`
// is the server's bytes-per-pixel. Returns complete=false (consumed=0) when the
// buffer is too short to hold the whole rectangle, so a streaming reader can
// wait for more data. Unknown encodings return complete=false. Pure/testable.
RectData decodeRect(const Rectangle& r, const QByteArray& buf, int off, int bpp = 4);

// Encode a SetEncodings client message (type 2) advertising `encodings` in
// preference order (most-preferred first).
QByteArray encodeSetEncodings(const QList<qint32>& encodings);

// Persistent zlib inflate stream for ZRLE (RFB 7.7.5). One instance per VNC
// connection: the compressed framebuffer stream is CONTINUOUS across every ZRLE
// rectangle, so the inflate context must survive between decodeZRLERect calls —
// resetting it per-rectangle corrupts the stream.
class RfbZlibStream {
public:
    RfbZlibStream();
    ~RfbZlibStream();
    RfbZlibStream(const RfbZlibStream&) = delete;
    RfbZlibStream& operator=(const RfbZlibStream&) = delete;
    // Inflate `compressed` and return every output byte it produced. Sets
    // failed() on a zlib error; state carries over between calls.
    QByteArray inflate(const QByteArray& compressed);
    bool failed() const { return m_failed; }
private:
    struct Impl;
    Impl* d;
    bool m_failed = false;
};

// Decode one ZRLE-encoded rectangle. `z` is the connection's persistent inflate
// stream (state carries across rectangles). The rectangle payload is a CARD32
// length then that many zlib-compressed bytes; inflated it holds 64×64 tiles,
// each with its own subencoding (raw / solid / packed-palette / plain-RLE /
// palette-RLE). Returns complete=false (consumed=0) if the buffer lacks the
// whole compressed block. `bpp` is the server bytes-per-pixel (a 32bpp/depth-24
// pixel uses a 3-byte CPIXEL). Pure w.r.t. `buf`; mutates only `z`.
RectData decodeZRLERect(RfbZlibStream& z, const Rectangle& r,
                        const QByteArray& buf, int off, int bpp = 4);

// Encode an RFB PointerEvent (client message-type 5): button-mask byte then the
// x,y position as big-endian uint16 (negatives clamped to 0). buttonMask: bit0
// left, bit1 middle, bit2 right, bits 3/4 wheel up/down.
QByteArray encodePointerEvent(int x, int y, int buttonMask);

// Encode an RFB KeyEvent (client message-type 4): down-flag byte, two padding
// bytes, then the X11 keysym as a big-endian uint32.
QByteArray encodeKeyEvent(quint32 keysym, bool down);

} // namespace rfb
} // namespace macxterm::connect
