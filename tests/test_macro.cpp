#include "core/Macro.h"
#include <QtTest/QtTest>

using namespace macxterm::core;

class TestMacro : public QObject {
    Q_OBJECT
private slots:
    void recordThenReplay() {
        Macro m("deploy");
        m.beginRecording();
        m.record("cd /srv\n");
        m.record("git pull\n");
        m.endRecording();
        QCOMPARE(m.eventCount(), 2);

        QByteArray out;
        m.replay([&](const QByteArray& e){ out += e; });
        QCOMPARE(out, QByteArray("cd /srv\ngit pull\n"));
    }

    void ignoresRecordWhenNotRecording() {
        Macro m("x");
        m.record("ignored");
        QCOMPARE(m.eventCount(), 0);
    }

    void serializeRoundTrip() {
        Macro m("build");
        m.beginRecording();
        m.record("make\n");
        m.record("make test\n");
        m.endRecording();

        const QByteArray blob = m.serialize();
        Macro m2 = Macro::deserialize("build", blob);
        QCOMPARE(m2.eventCount(), 2);
        QByteArray out;
        m2.replay([&](const QByteArray& e){ out += e; });
        QCOMPARE(out, QByteArray("make\nmake test\n"));
    }
};

QTEST_APPLESS_MAIN(TestMacro)
#include "test_macro.moc"
