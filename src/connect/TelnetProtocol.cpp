#include "connect/TelnetProtocol.h"

namespace macxterm::connect {
using namespace telnet;

void TelnetProtocol::negotiate(unsigned char verb, unsigned char opt, QByteArray& resp) {
    auto reply = [&](unsigned char v) {
        resp.append(static_cast<char>(IAC));
        resp.append(static_cast<char>(v));
        resp.append(static_cast<char>(opt));
    };
    switch (verb) {
        case DO:   // server asks us to enable `opt`
            reply(opt == OPT_SGA ? WILL : WONT);
            break;
        case DONT:
            reply(WONT);
            break;
        case WILL: // server offers to enable `opt`
            reply((opt == OPT_ECHO || opt == OPT_SGA) ? DO : DONT);
            break;
        case WONT:
            reply(DONT);
            break;
        default: break;
    }
}

TelnetProtocol::Result TelnetProtocol::process(const QByteArray& in) {
    Result r;
    unsigned char verb = 0;
    for (unsigned char b : in) {
        switch (m_st) {
            case St::Data:
                if (b == IAC) m_st = St::Iac;
                else r.appData.append(static_cast<char>(b));
                break;
            case St::Iac:
                if (b == IAC) { r.appData.append(static_cast<char>(IAC)); m_st = St::Data; }  // escaped 0xFF
                else if (b == WILL) m_st = St::Will;
                else if (b == WONT) m_st = St::Wont;
                else if (b == DO)   m_st = St::Do;
                else if (b == DONT) m_st = St::Dont;
                else if (b == SB)   m_st = St::Sb;
                else m_st = St::Data;   // 2-byte command (e.g. GA), ignore
                break;
            case St::Will: verb = WILL; negotiate(verb, b, r.response); m_st = St::Data; break;
            case St::Wont: verb = WONT; negotiate(verb, b, r.response); m_st = St::Data; break;
            case St::Do:   verb = DO;   negotiate(verb, b, r.response); m_st = St::Data; break;
            case St::Dont: verb = DONT; negotiate(verb, b, r.response); m_st = St::Data; break;
            case St::Sb:   if (b == IAC) m_st = St::SbIac; break;  // consume subnegotiation
            case St::SbIac: m_st = (b == SE) ? St::Data : St::Sb; break;
        }
    }
    return r;
}

} // namespace macxterm::connect
