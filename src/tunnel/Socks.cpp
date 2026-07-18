#include "tunnel/Socks.h"

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <basetsd.h>
using ssize_t = SSIZE_T;   // POSIX type used by the (shared) recv/send loop
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

namespace macxterm::tunnel {
namespace {
// Read exactly n bytes from a blocking fd. Returns false on EOF/error.
bool readN(int fd, char* buf, int n) {
    int off = 0;
    while (off < n) {
        const ssize_t r = ::recv(fd, buf + off, n - off, 0);
        if (r <= 0) return false;
        off += static_cast<int>(r);
    }
    return true;
}
} // namespace

bool socksNegotiate(int fd, QByteArray& outHost, int& outPort) {
    unsigned char ver = 0;
    if (!readN(fd, reinterpret_cast<char*>(&ver), 1)) return false;

    if (ver == 0x05) {
        // Greeting: nmethods, methods[nmethods].
        unsigned char nm = 0;
        if (!readN(fd, reinterpret_cast<char*>(&nm), 1)) return false;
        QByteArray methods(nm, 0);
        if (nm && !readN(fd, methods.data(), nm)) return false;
        const unsigned char reply[2] = {0x05, 0x00};   // no authentication
        ::send(fd, reinterpret_cast<const char*>(reply), 2, 0);
        // Request: VER CMD RSV ATYP ...
        unsigned char hdr[4];
        if (!readN(fd, reinterpret_cast<char*>(hdr), 4)) return false;
        if (hdr[1] != 0x01) return false;              // only CONNECT
        if (hdr[3] == 0x01) {                          // IPv4
            unsigned char ip[4];
            if (!readN(fd, reinterpret_cast<char*>(ip), 4)) return false;
            outHost = QByteArray::number(ip[0]) + "." + QByteArray::number(ip[1]) + "." +
                      QByteArray::number(ip[2]) + "." + QByteArray::number(ip[3]);
        } else if (hdr[3] == 0x03) {                   // domain name
            unsigned char len = 0;
            if (!readN(fd, reinterpret_cast<char*>(&len), 1)) return false;
            outHost.resize(len);
            if (len && !readN(fd, outHost.data(), len)) return false;
        } else if (hdr[3] == 0x04) {                   // IPv6
            unsigned char ip6[16];
            if (!readN(fd, reinterpret_cast<char*>(ip6), 16)) return false;
            char s[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, ip6, s, sizeof(s));
            outHost = s;
        } else {
            return false;
        }
        unsigned char pb[2];
        if (!readN(fd, reinterpret_cast<char*>(pb), 2)) return false;
        outPort = (pb[0] << 8) | pb[1];
        // Success reply: VER REP RSV ATYP BND.ADDR(4) BND.PORT(2).
        const unsigned char ok[10] = {0x05, 0x00, 0x00, 0x01, 0, 0, 0, 0, 0, 0};
        ::send(fd, reinterpret_cast<const char*>(ok), 10, 0);
        return true;
    }

    if (ver == 0x04) {
        // CMD(1) PORT(2) IP(4) USERID(null-terminated) [HOST(null-term) for 4a]
        unsigned char req[7];
        if (!readN(fd, reinterpret_cast<char*>(req), 7)) return false;
        if (req[0] != 0x01) return false;              // only CONNECT
        outPort = (req[1] << 8) | req[2];
        // Consume the (ignored) null-terminated userid.
        char c = 0;
        do { if (!readN(fd, &c, 1)) return false; } while (c != 0);
        const bool socks4a = (req[3] == 0 && req[4] == 0 && req[5] == 0 && req[6] != 0);
        if (socks4a) {
            outHost.clear();
            do { if (!readN(fd, &c, 1)) return false; if (c) outHost.append(c); } while (c != 0);
        } else {
            outHost = QByteArray::number(req[3]) + "." + QByteArray::number(req[4]) + "." +
                      QByteArray::number(req[5]) + "." + QByteArray::number(req[6]);
        }
        // Reply: VN=0 CD=0x5A (granted) + 6 bytes.
        const unsigned char ok[8] = {0x00, 0x5A, 0, 0, 0, 0, 0, 0};
        ::send(fd, reinterpret_cast<const char*>(ok), 8, 0);
        return true;
    }
    return false;
}

} // namespace macxterm::tunnel
