#pragma once
#include <QByteArray>
#include <QList>

namespace macxterm::connect::xdmcp {

// Minimal XDMCP (X Display Manager Control Protocol, UDP port 177) codec —
// enough for the discovery handshake that finds a willing display manager:
// the client broadcasts/sends a Query and the manager answers Willing. Full
// session bring-up (Request → Accept → Manage, then launching a local X server
// for the remote display) needs a live display manager and is out of scope
// here; this codec is the byte-exact, unit-tested foundation for it.
//
// Wire format: every integer is network byte order (big-endian). A packet is a
// 6-byte header — CARD16 version(=1), CARD16 opcode, CARD16 length (bytes that
// follow) — then the opcode-specific body. ARRAY8 = CARD8 length + bytes.

enum Opcode : quint16 {
    BroadcastQuery = 1, Query = 2, IndirectQuery = 3, ForwardQuery = 4,
    Willing = 5, Unwilling = 6, Request = 7, Accept = 8, Decline = 9,
    Manage = 10, Refuse = 11, Failed = 12, KeepAlive = 13, Alive = 14,
};

constexpr quint16 kVersion = 1;

// A parsed packet header.
struct Header {
    quint16 version = 0;
    quint16 opcode = 0;
    quint16 length = 0;   // number of body bytes that should follow the header
    bool valid = false;
};

// Parse the 6-byte header at the front of `buf`. Invalid if too short.
Header parseHeader(const QByteArray& buf);

// Encode a Query packet advertising the given authentication names (usually
// empty for "no authentication"). Body is an ARRAYofARRAY8: a CARD8 count then
// each name as an ARRAY8.
QByteArray encodeQuery(const QList<QByteArray>& authNames = {});

// The three ARRAY8 fields a Willing reply carries.
struct WillingInfo {
    QByteArray authenticationName;
    QByteArray hostname;      // display manager's greeting/host string
    QByteArray status;        // human-readable status ("Willing to manage")
    bool valid = false;
};

// Parse a Willing packet (header opcode must be Willing). Returns valid=false
// if the opcode is wrong or the body is truncated.
WillingInfo parseWilling(const QByteArray& buf);

} // namespace macxterm::connect::xdmcp
