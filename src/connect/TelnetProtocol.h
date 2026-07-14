#pragma once
#include <QByteArray>

namespace macxterm::connect {

// Telnet (RFC 854) command bytes.
namespace telnet {
constexpr unsigned char SE = 240, SB = 250;
constexpr unsigned char WILL = 251, WONT = 252, DO = 253, DONT = 254, IAC = 255;
constexpr unsigned char OPT_ECHO = 1, OPT_SGA = 3, OPT_NAWS = 31, OPT_TTYPE = 24;
}

// Stateless Telnet option-negotiation processor. Given a raw inbound byte
// stream it strips IAC command sequences, returns the plain application data
// for the terminal, and produces the required IAC responses to send back.
// Negotiation policy: we refuse most options (DONT/WONT) but accept SGA and
// server ECHO, which is the minimal interoperable set for a line-mode client.
class TelnetProtocol {
public:
    struct Result {
        QByteArray appData;   // bytes to feed the terminal (IAC removed)
        QByteArray response;  // IAC bytes to write back to the server
    };

    // Process one chunk. Internal state carries a partial IAC sequence across
    // calls, so streaming works.
    Result process(const QByteArray& in);

private:
    enum class St { Data, Iac, Will, Wont, Do, Dont, Sb, SbIac };
    St m_st = St::Data;
    void negotiate(unsigned char verb, unsigned char opt, QByteArray& resp);
};

} // namespace macxterm::connect
