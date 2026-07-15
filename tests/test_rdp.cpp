#include "connect/RdpConnection.h"
#include <QtTest/QtTest>

using namespace macxterm;

class TestRdp : public QObject {
    Q_OBJECT
private slots:
    void basicArgs() {
        core::Session s("r", core::SessionType::Rdp);
        s.setHost("10.0.0.5");
        s.setUsername("admin");
        auto args = connect::RdpConnection::buildFreeRdpArgs(s);
        QVERIFY(args.contains("/v:10.0.0.5:3389"));   // default RDP port
        QVERIFY(args.contains("/u:admin"));
        QVERIFY(args.contains("+clipboard"));         // on by default
    }

    void adminAndDriveOptions() {
        core::Session s("r", core::SessionType::Rdp);
        s.setHost("h");
        s.setParam("admin", "1");
        s.setParam("drives", "1");
        auto args = connect::RdpConnection::buildFreeRdpArgs(s);
        QVERIFY(args.contains("/admin"));
        QVERIFY(args.join(' ').contains("/drive:"));
    }

    void clipboardCanBeDisabled() {
        core::Session s("r", core::SessionType::Rdp);
        s.setHost("h");
        s.setParam("clipboard", "0");
        auto args = connect::RdpConnection::buildFreeRdpArgs(s);
        QVERIFY(!args.contains("+clipboard"));
    }

    void capabilitiesGui() {
        connect::RdpConnection c;
        QVERIFY(c.capabilities().gui);   // RDP renders its own surface
    }

#ifdef MACXTERM_HAVE_FREERDP
    void keysymToScancodeMapping() {
        bool ext = false;
        // Named keys map to a non-zero set-1 scancode.
        QVERIFY(connect::RdpConnection::keysymToRdpScancode(0xff0d, &ext) != 0);  // Return
        QCOMPARE(ext, false);
        QVERIFY(connect::RdpConnection::keysymToRdpScancode(0xff08, &ext) != 0);  // Backspace
        QCOMPARE(ext, false);
        // Arrow/nav keys set the extended flag.
        QVERIFY(connect::RdpConnection::keysymToRdpScancode(0xff51, &ext) != 0);  // Left
        QCOMPARE(ext, true);
        QVERIFY(connect::RdpConnection::keysymToRdpScancode(0xffff, &ext) != 0);  // Delete
        QCOMPARE(ext, true);
        // Printable characters are not mapped (sent as Unicode instead).
        QCOMPARE(connect::RdpConnection::keysymToRdpScancode(0x41, &ext), 0);     // 'A'
    }
#endif
};

QTEST_APPLESS_MAIN(TestRdp)
#include "test_rdp.moc"
