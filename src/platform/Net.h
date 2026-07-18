#pragma once
// Cross-platform BSD-socket helpers (Architecture §5, PAL). These wrap the tiny
// set of raw-socket operations the SSH family needs so the same libssh2 code can
// run over Winsock on Windows and BSD sockets on Unix.
//
// On Unix each function expands to exactly the syscall the code used before this
// shim existed (see Net.cpp), so macOS/Linux behaviour is byte-identical. On
// Windows the divergent parts (WSAStartup, closesocket, ioctlsocket, and the
// missing socketpair) live behind these entry points.
//
// File descriptors are kept as plain `int` on both platforms: on Windows a SOCKET
// is a small handle that fits in an int for these uses, and an invalid socket maps
// to -1, so the existing `fd < 0` / `fd >= 0` checks keep working unchanged.

namespace macxterm::platform::net {

// Idempotent Winsock initialisation. No-op on Unix. Safe to call repeatedly and
// from any thread; the first call in the process does the real WSAStartup.
void startup();

// Open a blocking TCP stream socket to host:port (host may be a name or literal
// address). Returns a connected fd, or -1 on failure. Mirrors the old openSocket.
int connectTcp(const char* host, int port);

// Create a connected stream-socket pair into sp[2]. Returns 0 on success, -1 on
// failure. Unix: AF_UNIX socketpair(). Windows: a loopback-TCP emulation (Windows
// has no socketpair). Used to bridge a libssh2 channel to a local fd (jump hosts).
int socketPair(int sp[2]);

// Close a socket fd (closesocket on Windows, close on Unix).
void closeSocket(int fd);

// Shut down both directions of a socket (shutdown SHUT_RDWR / SD_BOTH) — used to
// unblock a thread parked in a blocking read so it can be joined on teardown.
void shutdownBoth(int fd);

// Put a socket fd into non-blocking mode (ioctlsocket FIONBIO on Windows,
// fcntl O_NONBLOCK on Unix).
void setNonBlocking(int fd);

// Wait up to timeoutMs for the socket to become readable. Returns >0 if readable,
// 0 on timeout, -1 on error. Unix: poll(POLLIN). Windows: WSAPoll(POLLRDNORM).
int pollReadable(int fd, int timeoutMs);

// Duplicate a socket fd into an independent handle the caller owns (so a worker
// thread can keep using it after the original wrapper is closed). Returns the new
// fd or -1. Unix: dup(). Windows: WSADuplicateSocket into the same process.
int dupSocket(int fd);

} // namespace macxterm::platform::net
