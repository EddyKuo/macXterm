#include "connect/SerialConnection.h"
#include "core/Session.h"
#include <QtTest/QtTest>

#if !defined(_WIN32)
#if defined(__APPLE__) || defined(__FreeBSD__)
#include <util.h>       // openpty on macOS/BSD
#else
#include <pty.h>        // openpty on Linux (glibc)
#endif
#include <unistd.h>
#endif

using namespace macxterm;

// Serial e2e using a real pseudo-terminal pair as a stand-in for a serial
// device (no physical adapter needed): QSerialPort opens the slave tty; we
// write to the master fd and verify SerialConnection reads it. This exercises
// the actual QSerialPort read path over a genuine kernel tty.
class TestSerialPty : public QObject {
    Q_OBJECT
private slots:
    void readsFromPtySerialDevice() {
#if defined(_WIN32)
        QSKIP("openpty-based serial simulation is POSIX-only");
#else
        int master = -1, slave = -1;
        char name[128];
        if (openpty(&master, &slave, name, nullptr, nullptr) != 0)
            QSKIP("openpty unavailable in this environment");
        const QString devPath = QString::fromLocal8Bit(name);

        connect::SerialConnection conn;
        core::Session s("com", core::SessionType::Serial);
        s.setParam("port", devPath);
        s.setParam("baud", "9600");
        if (!conn.connectSession(s)) {
            ::close(master); ::close(slave);
            QSKIP("QSerialPort could not open the pty slave on this platform");
        }
        QCOMPARE(conn.state(), connect::IConnection::State::Connected);

        QByteArray got;
        connect(&conn, &connect::IConnection::dataReceived, [&](const QByteArray& d){ got += d; });

        const char* payload = "SERIAL_PTY_OK\n";
        QVERIFY(::write(master, payload, qstrlen(payload)) > 0);

        QTRY_VERIFY_WITH_TIMEOUT(got.contains("SERIAL_PTY_OK"), 3000);
        conn.disconnectSession();
        ::close(master); ::close(slave);
#endif
    }
};

QTEST_GUILESS_MAIN(TestSerialPty)
#include "test_serial_pty.moc"
