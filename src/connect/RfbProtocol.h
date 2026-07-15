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

// Encode an RFB PointerEvent (client message-type 5): button-mask byte then the
// x,y position as big-endian uint16 (negatives clamped to 0). buttonMask: bit0
// left, bit1 middle, bit2 right, bits 3/4 wheel up/down.
QByteArray encodePointerEvent(int x, int y, int buttonMask);

// Encode an RFB KeyEvent (client message-type 4): down-flag byte, two padding
// bytes, then the X11 keysym as a big-endian uint32.
QByteArray encodeKeyEvent(quint32 keysym, bool down);

} // namespace rfb
} // namespace macxterm::connect
