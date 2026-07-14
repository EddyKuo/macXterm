#include "tools/TftpPacket.h"

namespace macxterm::tools::tftp {

static quint16 rd16(const QByteArray& b, int off) {
    return (static_cast<quint8>(b[off]) << 8) | static_cast<quint8>(b[off + 1]);
}
static void wr16(QByteArray& b, quint16 v) {
    b.append(static_cast<char>((v >> 8) & 0xff));
    b.append(static_cast<char>(v & 0xff));
}

Packet decode(const QByteArray& b) {
    Packet p;
    if (b.size() < 2) return p;
    const quint16 op = rd16(b, 0);
    switch (op) {
        case 1: case 2: {  // RRQ / WRQ: opcode, filename\0mode\0
            p.op = (op == 1) ? Op::RRQ : Op::WRQ;
            const int fnEnd = b.indexOf('\0', 2);
            if (fnEnd < 0) return p;
            p.filename = QString::fromUtf8(b.mid(2, fnEnd - 2));
            const int mEnd = b.indexOf('\0', fnEnd + 1);
            if (mEnd < 0) return p;
            p.mode = QString::fromUtf8(b.mid(fnEnd + 1, mEnd - fnEnd - 1));
            p.valid = true;
            break;
        }
        case 3:  // DATA: opcode, block, data
            if (b.size() < 4) return p;
            p.op = Op::DATA; p.block = rd16(b, 2); p.data = b.mid(4); p.valid = true;
            break;
        case 4:  // ACK: opcode, block
            if (b.size() < 4) return p;
            p.op = Op::ACK; p.block = rd16(b, 2); p.valid = true;
            break;
        case 5: {  // ERROR: opcode, code, msg\0
            if (b.size() < 4) return p;
            p.op = Op::ERROR; p.errorCode = rd16(b, 2);
            const int end = b.indexOf('\0', 4);
            p.errorMsg = QString::fromUtf8(b.mid(4, end < 0 ? -1 : end - 4));
            p.valid = true;
            break;
        }
        default: break;
    }
    return p;
}

QByteArray encodeAck(quint16 block) {
    QByteArray b; wr16(b, 4); wr16(b, block); return b;
}

QByteArray encodeData(quint16 block, const QByteArray& data) {
    QByteArray b; wr16(b, 3); wr16(b, block); b.append(data); return b;
}

QByteArray encodeError(quint16 code, const QString& msg) {
    QByteArray b; wr16(b, 5); wr16(b, code); b.append(msg.toUtf8()); b.append('\0'); return b;
}

} // namespace macxterm::tools::tftp
