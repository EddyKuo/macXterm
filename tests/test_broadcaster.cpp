#include "core/InputBroadcaster.h"
#include <QtTest/QtTest>

using namespace macxterm::core;

class TestBroadcaster : public QObject {
    Q_OBJECT
private slots:
    void broadcastsToAllEnabled() {
        InputBroadcaster b;
        QByteArray a1, a2, a3;
        b.addTarget([&](const QByteArray& d){ a1 += d; });
        b.addTarget([&](const QByteArray& d){ a2 += d; });
        b.addTarget([&](const QByteArray& d){ a3 += d; });
        QCOMPARE(b.broadcast("ls\n"), 3);
        QCOMPARE(a1, QByteArray("ls\n"));
        QCOMPARE(a2, QByteArray("ls\n"));
        QCOMPARE(a3, QByteArray("ls\n"));
    }

    void disabledTargetOptsOut() {
        InputBroadcaster b;
        QByteArray a1, a2;
        int id1 = b.addTarget([&](const QByteArray& d){ a1 += d; });
        b.addTarget([&](const QByteArray& d){ a2 += d; });
        b.setEnabled(id1, false);
        QCOMPARE(b.enabledCount(), 1);
        QCOMPARE(b.broadcast("x"), 1);
        QCOMPARE(a1, QByteArray());        // opted out
        QCOMPARE(a2, QByteArray("x"));
    }

    void removedTargetGone() {
        InputBroadcaster b;
        int id = b.addTarget([](const QByteArray&){});
        b.addTarget([](const QByteArray&){});
        b.removeTarget(id);
        QCOMPARE(b.targetCount(), 1);
        QCOMPARE(b.broadcast("y"), 1);
    }
};

QTEST_APPLESS_MAIN(TestBroadcaster)
#include "test_broadcaster.moc"
