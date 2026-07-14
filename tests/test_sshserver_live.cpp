#include "tools/SshServer.h"
#include <QtTest/QtTest>
#include <QTemporaryDir>

#if !defined(_WIN32)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#endif

using namespace macxterm::tools;

// Integration smoke test for the embedded SSH/SFTP server: start it on loopback
// and confirm it accepts a TCP connection and emits a valid SSH identification
// banner ("SSH-2.0-..."). We use a raw socket rather than a libssh2 client on
// purpose — linking libssh (server) and libssh2 (client) into the *same* test
// process and running both crypto stacks concurrently corrupts shared OpenSSL
// state. A real external client (ssh/sftp, or macXterm's own SshConnection in a
// separate process) exercises the full auth + SFTP path.
// Skipped unless built with libssh (MACXTERM_HAVE_LIBSSH).
class TestSshServerLive : public QObject {
    Q_OBJECT
private slots:
    void emitsSshBanner() {
#if !defined(MACXTERM_HAVE_LIBSSH) || defined(_WIN32)
        QSKIP("built without libssh");
#else
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const quint16 port = 22236;
        SshServer server;
        QVERIFY(server.start(port, "user", "pass", dir.path()));
        QVERIFY(server.isRunning());
        QTest::qWait(400);   // let the accept loop come up

        const int sock = ::socket(AF_INET, SOCK_STREAM, 0);
        QVERIFY(sock >= 0);
        sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons(port);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
        QCOMPARE(::connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0);

        // The SSH server sends its identification string on connect.
        char buf[256] = {0};
        struct pollfd pfd{sock, POLLIN, 0};
        const int pr = ::poll(&pfd, 1, 2000);
        QVERIFY2(pr > 0, "no banner within 2s");
        const ssize_t n = ::read(sock, buf, sizeof(buf) - 1);
        QVERIFY(n > 0);
        const QByteArray banner(buf, static_cast<int>(n));
        QVERIFY2(banner.startsWith("SSH-2.0"), banner.constData());

        ::close(sock);
        server.stop();
        QVERIFY(!server.isRunning());
#endif
    }

    void startStopIsClean() {
#if !defined(MACXTERM_HAVE_LIBSSH) || defined(_WIN32)
        QSKIP("built without libssh");
#else
        SshServer server;
        QVERIFY(server.start(22237, "u", "p", QDir::tempPath()));
        QTest::qWait(150);
        server.stop();
        QVERIFY(!server.isRunning());
        // A second cycle must also work (no leaked bind).
        QVERIFY(server.start(22237, "u", "p", QDir::tempPath()));
        QTest::qWait(150);
        server.stop();
#endif
    }
};

QTEST_MAIN(TestSshServerLive)
#include "test_sshserver_live.moc"
