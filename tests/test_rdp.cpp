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
};

QTEST_APPLESS_MAIN(TestRdp)
#include "test_rdp.moc"
