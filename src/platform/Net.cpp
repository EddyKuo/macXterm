#include "platform/Net.h"

#if defined(_WIN32)
// ── Windows: Winsock2 ──
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mutex>

namespace macxterm::platform::net {

void startup() {
    // WSAStartup is refcounted, but do it exactly once so we never depend on a
    // matching WSACleanup — the app needs Winsock up for its whole lifetime.
    static std::once_flag once;
    std::call_once(once, [] {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
    });
}

int connectTcp(const char* host, int port) {
    startup();
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    char portStr[16];
    std::snprintf(portStr, sizeof(portStr), "%d", port);
    if (getaddrinfo(host, portStr, &hints, &res) != 0) return -1;
    SOCKET fd = INVALID_SOCKET;
    for (auto* p = res; p; p = p->ai_next) {
        fd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd == INVALID_SOCKET) continue;
        if (::connect(fd, p->ai_addr, static_cast<int>(p->ai_addrlen)) == 0) break;
        ::closesocket(fd);
        fd = INVALID_SOCKET;
    }
    freeaddrinfo(res);
    return fd == INVALID_SOCKET ? -1 : static_cast<int>(fd);
}

int socketPair(int sp[2]) {
    startup();
    // Emulate socketpair() over a loopback TCP connection: bind a listener to an
    // ephemeral port on 127.0.0.1, connect to it, accept, and hand back the two
    // connected ends. Good enough as an in-process byte pipe for the jump relay.
    SOCKET listener = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) return -1;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    int len = sizeof(addr);
    if (::bind(listener, reinterpret_cast<sockaddr*>(&addr), len) != 0 ||
        ::getsockname(listener, reinterpret_cast<sockaddr*>(&addr), &len) != 0 ||
        ::listen(listener, 1) != 0) {
        ::closesocket(listener);
        return -1;
    }

    SOCKET client = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client == INVALID_SOCKET) { ::closesocket(listener); return -1; }
    if (::connect(client, reinterpret_cast<sockaddr*>(&addr), len) != 0) {
        ::closesocket(client); ::closesocket(listener); return -1;
    }
    SOCKET server = ::accept(listener, nullptr, nullptr);
    ::closesocket(listener);
    if (server == INVALID_SOCKET) { ::closesocket(client); return -1; }

    sp[0] = static_cast<int>(client);
    sp[1] = static_cast<int>(server);
    return 0;
}

void closeSocket(int fd) {
    if (fd >= 0) ::closesocket(static_cast<SOCKET>(fd));
}

void shutdownBoth(int fd) {
    if (fd >= 0) ::shutdown(static_cast<SOCKET>(fd), SD_BOTH);
}

void setNonBlocking(int fd) {
    u_long on = 1;
    ::ioctlsocket(static_cast<SOCKET>(fd), FIONBIO, &on);
}

int pollReadable(int fd, int timeoutMs) {
    WSAPOLLFD pfd{};
    pfd.fd = static_cast<SOCKET>(fd);
    pfd.events = POLLRDNORM;
    const int r = ::WSAPoll(&pfd, 1, timeoutMs);
    if (r <= 0) return r;
    return (pfd.revents & (POLLRDNORM | POLLHUP | POLLERR)) ? 1 : 0;
}

int dupSocket(int fd) {
    WSAPROTOCOL_INFOW info{};
    if (::WSADuplicateSocketW(static_cast<SOCKET>(fd), ::GetCurrentProcessId(), &info) != 0)
        return -1;
    const SOCKET s = ::WSASocketW(info.iAddressFamily, info.iSocketType, info.iProtocol,
                                  &info, 0, 0);
    return s == INVALID_SOCKET ? -1 : static_cast<int>(s);
}

} // namespace macxterm::platform::net

#else
// ── Unix: BSD sockets. Each function is the exact call the code used before the
// shim, so macOS/Linux behaviour is unchanged. ──
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <cstdio>

namespace macxterm::platform::net {

void startup() {}

int connectTcp(const char* host, int port) {
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    char portStr[16];
    std::snprintf(portStr, sizeof(portStr), "%d", port);
    if (getaddrinfo(host, portStr, &hints, &res) != 0) return -1;
    int fd = -1;
    for (auto* p = res; p; p = p->ai_next) {
        fd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;
        if (::connect(fd, p->ai_addr, p->ai_addrlen) == 0) break;
        ::close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    return fd;
}

int socketPair(int sp[2]) {
    return ::socketpair(AF_UNIX, SOCK_STREAM, 0, sp);
}

void closeSocket(int fd) {
    if (fd >= 0) ::close(fd);
}

void shutdownBoth(int fd) {
    if (fd >= 0) ::shutdown(fd, SHUT_RDWR);
}

void setNonBlocking(int fd) {
    ::fcntl(fd, F_SETFL, O_NONBLOCK);
}

int pollReadable(int fd, int timeoutMs) {
    struct pollfd pfd{fd, POLLIN, 0};
    const int r = ::poll(&pfd, 1, timeoutMs);
    if (r <= 0) return r;
    return (pfd.revents & POLLIN) ? 1 : 0;
}

int dupSocket(int fd) {
    return ::dup(fd);
}

} // namespace macxterm::platform::net
#endif
