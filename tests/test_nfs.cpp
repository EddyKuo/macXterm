#include "tools/Xdr.h"
#include "tools/NfsServer.h"
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>

using namespace macxterm::tools;

// Build a minimal ONC-RPC CALL datagram (AUTH_NONE cred/verf).
static QByteArray rpcCall(quint32 xid, quint32 prog, quint32 vers, quint32 proc,
                          const QByteArray& args = {}) {
    XdrWriter w;
    w.u32(xid); w.u32(0); w.u32(2);          // xid, CALL, rpcvers=2
    w.u32(prog); w.u32(vers); w.u32(proc);
    w.u32(0); w.u32(0);                       // cred: AUTH_NONE, len 0
    w.u32(0); w.u32(0);                       // verf: AUTH_NONE, len 0
    QByteArray d = w.data();
    d.append(args);
    return d;
}

class TestNfs : public QObject {
    Q_OBJECT
private slots:
    // ── XDR codec ──
    void xdrRoundTrip() {
        XdrWriter w;
        w.u32(0x01020304u);
        w.u64(0x1122334455667788ull);
        w.opaqueVar(QByteArray("abc"));       // 3 bytes → padded to 4
        w.str(QStringLiteral("hi"));
        XdrReader r(w.data());
        bool ok = false;
        QCOMPARE(r.u32(&ok), 0x01020304u); QVERIFY(ok);
        QCOMPARE(r.u64(&ok), 0x1122334455667788ull); QVERIFY(ok);
        QCOMPARE(r.opaqueVar(&ok), QByteArray("abc")); QVERIFY(ok);
        QCOMPARE(r.str(&ok), QStringLiteral("hi")); QVERIFY(ok);
    }

    void xdrPadsToFourBytes() {
        XdrWriter w;
        w.opaqueVar(QByteArray("x"));         // 4 (len) + 1 + 3 pad = 8
        QCOMPARE(w.data().size(), 8);
    }

    void xdrShortReadFails() {
        XdrReader r(QByteArray("\x00\x00", 2));
        bool ok = true;
        r.u32(&ok);
        QVERIFY(!ok);
    }

    // ── RPC dispatch ──
    void portmapGetPortReturnsNfsPort() {
        NfsServer s;
        QTemporaryDir dir;
        QVERIFY(s.start(dir.path(), 2049));
        XdrWriter args; args.u32(100003); args.u32(3); args.u32(17); args.u32(0);
        const QByteArray reply = s.handleDatagram(rpcCall(42, 100000, 2, 3, args.data()));
        XdrReader r(reply);
        bool ok = false;
        QCOMPARE(r.u32(&ok), 42u);            // xid echoed
        r.u32(&ok); r.u32(&ok);               // REPLY, MSG_ACCEPTED
        r.u32(&ok); r.u32(&ok);               // verf
        r.u32(&ok);                           // accept_stat = SUCCESS
        QCOMPARE(r.u32(&ok), 2049u);          // GETPORT result
        s.stop();
    }

    void mountReturnsRootHandleAndNfsBrowses() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        { QFile f(dir.filePath("file.txt")); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("hello"); }

        NfsServer s;
        QVERIFY(s.start(dir.path(), 2049));

        // MNT → root file handle.
        XdrWriter mntArgs; mntArgs.str(dir.path());
        const QByteArray mnt = s.handleDatagram(rpcCall(1, 100005, 3, 1, mntArgs.data()));
        XdrReader mr(mnt);
        bool ok = false;
        mr.u32(&ok); mr.u32(&ok); mr.u32(&ok); mr.u32(&ok); mr.u32(&ok); mr.u32(&ok);  // rpc header
        QCOMPARE(mr.u32(&ok), 0u);            // mount status OK
        const QByteArray rootFh = mr.opaqueVar(&ok);
        QVERIFY(ok);
        QCOMPARE(rootFh.size(), 32);

        // READDIR(root) must list our file.
        XdrWriter rdArgs;
        rdArgs.opaqueVar(rootFh);
        rdArgs.u64(0);                        // cookie
        rdArgs.opaqueFixed(QByteArray(8, '\0'));  // cookieverf
        rdArgs.u32(8192);                     // count
        const QByteArray rd = s.handleDatagram(rpcCall(2, 100003, 3, 16, rdArgs.data()));
        QVERIFY(rd.contains("file.txt"));
        s.stop();
    }

    void unknownProgramYieldsNoReply() {
        NfsServer s;
        QTemporaryDir dir;
        QVERIFY(s.start(dir.path(), 2049));
        QVERIFY(s.handleDatagram(rpcCall(1, 999999, 1, 1)).isEmpty());
        s.stop();
    }
};

QTEST_MAIN(TestNfs)
#include "test_nfs.moc"
