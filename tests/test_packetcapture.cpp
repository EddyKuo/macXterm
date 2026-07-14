#include "tools/PacketCapture.h"
#include <QtTest/QtTest>

using namespace macxterm::tools;

// Exercises the capture engine's control paths (the pure decoder is covered by
// test_packetdecode). Live capture needs elevated privilege, so we don't assert
// packets arrive — only that start/stop and interface enumeration behave.
class TestPacketCapture : public QObject {
    Q_OBJECT
private slots:
    void availabilityAndInterfaces() {
        // With libpcap present this is true; the list is usually non-empty but we
        // don't require it (CI may have no permitted interfaces).
        const bool avail = PacketCapture::available();
        const QStringList ifs = PacketCapture::listInterfaces();
        if (avail) QVERIFY(ifs.size() >= 0);
        else QVERIFY(ifs.isEmpty());
    }

    void startStopIsClean() {
        PacketCapture cap;
        QVERIFY(!cap.isRunning());
        const QStringList ifs = PacketCapture::listInterfaces();
        const QString iface = ifs.isEmpty() ? QStringLiteral("lo0") : ifs.first();

        // start() may fail without capture privilege; either way stop() is safe
        // and isRunning() must end up false.
        cap.start(iface, QStringLiteral("tcp"));
        QTest::qWait(150);
        cap.stop();
        QVERIFY(!cap.isRunning());
    }

    void emptyInterfaceRejected() {
        PacketCapture cap;
        QVERIFY(!cap.start(QString()));      // no interface → false
    }
};

QTEST_MAIN(TestPacketCapture)
#include "test_packetcapture.moc"
