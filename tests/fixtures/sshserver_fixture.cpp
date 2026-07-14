// Standalone harness that runs the embedded SshServer, used by
// test_sshserver_sftp: a libssh2 client (in the test process) drives this libssh
// server (in a separate process) — avoiding the in-process dual-crypto-stack
// corruption that prevents linking both into one binary at runtime.
//
// Usage: sshserver_fixture <port> <user> <pass> <rootDir>
#include "tools/SshServer.h"
#include <QCoreApplication>
#include <QTimer>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <atomic>
#include <unistd.h>

#if defined(MACXTERM_COVERAGE)
extern "C" int __llvm_profile_write_file(void);   // provided by the profile runtime
#endif

static std::atomic<bool> g_term{false};
static void onTerm(int) { g_term.store(true); }

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (argc < 5) { std::fprintf(stderr, "usage: <port> <user> <pass> <dir>\n"); return 2; }

    auto* server = new macxterm::tools::SshServer;
    if (!server->start(static_cast<quint16>(std::atoi(argv[1])), argv[2], argv[3], argv[4])) {
        std::fprintf(stderr, "start failed\n");
        return 3;
    }
    std::fprintf(stderr, "READY\n");
    std::fflush(stderr);

    // Shut down on SIGTERM by first stopping the server — joining its worker
    // threads is a synchronization point that makes their coverage counters
    // visible to this thread. Only then flush the profile (racy mid-flight
    // writes recorded stale counts). Poll the flag from the event loop so the
    // heavy work happens in a normal context, not a signal handler.
    std::signal(SIGTERM, onTerm);
    auto* poll = new QTimer(&app);
    QObject::connect(poll, &QTimer::timeout, [server] {
        if (!g_term.load()) return;
        server->stop();
#if defined(MACXTERM_COVERAGE)
        __llvm_profile_write_file();
#endif
        _exit(0);
    });
    poll->start(100);
    return app.exec();
}
