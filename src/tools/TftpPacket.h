#pragma once
#include <QByteArray>
#include <QString>

namespace macxterm::tools {

// TFTP (RFC 1350) packet codec for the built-in light TFTP server
// (research §1.1 light daemons; no MobaXterm-Home 360s cap — PRD G3).
// Pure encode/decode so it is unit-testable without sockets.
namespace tftp {

enum class Op : quint16 { RRQ = 1, WRQ = 2, DATA = 3, ACK = 4, ERROR = 5, Invalid = 0 };

struct Packet {
    Op op = Op::Invalid;
    QString filename;   // RRQ/WRQ
    QString mode;       // RRQ/WRQ ("octet"/"netascii")
    quint16 block = 0;  // DATA/ACK
    QByteArray data;    // DATA
    quint16 errorCode = 0;
    QString errorMsg;
    bool valid = false;
};

Packet decode(const QByteArray& bytes);
QByteArray encodeAck(quint16 block);
QByteArray encodeData(quint16 block, const QByteArray& data);
QByteArray encodeError(quint16 code, const QString& msg);

} // namespace tftp
} // namespace macxterm::tools
