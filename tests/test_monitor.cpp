#include "tools/RemoteMonitor.h"
#include <QtTest/QtTest>

using namespace macxterm::tools::monitor;

class TestMonitor : public QObject {
    Q_OBJECT
private slots:
    void parsesMemInfo() {
        const QString sample =
            "MemTotal:       16384000 kB\n"
            "MemFree:         1000000 kB\n"
            "MemAvailable:    8192000 kB\n";
        MemInfo m = parseMemInfo(sample);
        QVERIFY(m.valid);
        QCOMPARE(m.totalKb, 16384000L);
        QCOMPARE(m.availKb, 8192000L);
        QVERIFY(qAbs(m.usedPercent - 50.0) < 0.01);
    }

    void parsesCpuStat() {
        CpuSample s = parseCpuStat("cpu  100 0 50 850 0 0 0 0");
        QVERIFY(s.valid);
        QCOMPARE(s.idle, 850L);
        QCOMPARE(s.total, 1000L);
    }

    void computesCpuBusyBetweenSamples() {
        CpuSample a; a.idle = 850; a.total = 1000; a.valid = true;
        CpuSample b; b.idle = 900; b.total = 1100; b.valid = true;
        // dTotal=100, dIdle=50 -> busy 50%
        QVERIFY(qAbs(cpuBusyPercent(a, b) - 50.0) < 0.01);
    }

    void invalidCpuLine() {
        QVERIFY(!parseCpuStat("garbage line").valid);
    }
};

QTEST_APPLESS_MAIN(TestMonitor)
#include "test_monitor.moc"
