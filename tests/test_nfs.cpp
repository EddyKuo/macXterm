#include "tools/Xdr.h"
#include "tools/NfsServer.h"
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>

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

    void createWriteAndReadBack() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        NfsServer s;
        QVERIFY(s.start(dir.path(), 2049));

        // Root fh via MNT.
        XdrWriter mntArgs; mntArgs.str(dir.path());
        XdrReader mr(s.handleDatagram(rpcCall(1, 100005, 3, 1, mntArgs.data())));
        bool ok = false;
        for (int i = 0; i < 6; ++i) mr.u32(&ok);
        mr.u32(&ok);                                   // status
        const QByteArray rootFh = mr.opaqueVar(&ok);
        QVERIFY(ok);

        // CREATE "new.txt" in root.
        XdrWriter cr;
        cr.opaqueVar(rootFh);
        cr.str(QStringLiteral("new.txt"));
        cr.u32(0);                                     // createmode UNCHECKED
        cr.u32(0); cr.u32(0); cr.u32(0); cr.u32(0); cr.u32(0); cr.u32(0);   // empty sattr3
        const QByteArray createReply = s.handleDatagram(rpcCall(2, 100003, 3, 8, cr.data()));
        // Parse the created file handle out of the reply.
        XdrReader crr(createReply);
        for (int i = 0; i < 6; ++i) crr.u32(&ok);      // rpc header
        QCOMPARE(crr.u32(&ok), 0u);                    // NFS3_OK
        QCOMPARE(crr.u32(&ok), 1u);                    // handle_follows
        const QByteArray fileFh = crr.opaqueVar(&ok);
        QVERIFY(ok);
        QVERIFY(QFile::exists(dir.filePath("new.txt")));

        // WRITE "hello" at offset 0.
        XdrWriter wr;
        wr.opaqueVar(fileFh);
        wr.u64(0);                                     // offset
        wr.u32(5);                                     // count
        wr.u32(2);                                     // stable = FILE_SYNC
        wr.opaqueVar(QByteArray("hello"));
        const QByteArray wReply = s.handleDatagram(rpcCall(3, 100003, 3, 7, wr.data()));
        XdrReader wrr(wReply);
        for (int i = 0; i < 6; ++i) wrr.u32(&ok);
        QCOMPARE(wrr.u32(&ok), 0u);                    // NFS3_OK

        // The file on disk now holds "hello".
        QFile f(dir.filePath("new.txt"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), QByteArray("hello"));
        s.stop();
    }

    void mkdirAndRemove() {
        QTemporaryDir dir;
        NfsServer s;
        QVERIFY(s.start(dir.path(), 2049));
        XdrWriter mntArgs; mntArgs.str(dir.path());
        XdrReader mr(s.handleDatagram(rpcCall(1, 100005, 3, 1, mntArgs.data())));
        bool ok = false;
        for (int i = 0; i < 6; ++i) mr.u32(&ok);
        mr.u32(&ok);
        const QByteArray rootFh = mr.opaqueVar(&ok);

        // MKDIR "sub".
        XdrWriter mk; mk.opaqueVar(rootFh); mk.str(QStringLiteral("sub"));
        for (int i = 0; i < 6; ++i) mk.u32(0);         // empty sattr3
        s.handleDatagram(rpcCall(2, 100003, 3, 9, mk.data()));
        QVERIFY(QFileInfo(dir.filePath("sub")).isDir());

        // RMDIR "sub".
        XdrWriter rm; rm.opaqueVar(rootFh); rm.str(QStringLiteral("sub"));
        s.handleDatagram(rpcCall(3, 100003, 3, 13, rm.data()));
        QVERIFY(!QFileInfo::exists(dir.filePath("sub")));
        s.stop();
    }

    void unknownProgramYieldsNoReply() {
        NfsServer s;
        QTemporaryDir dir;
        QVERIFY(s.start(dir.path(), 2049));
        QVERIFY(s.handleDatagram(rpcCall(1, 999999, 1, 1)).isEmpty());
        s.stop();
    }

    // Drive the remaining NFS/MOUNT/portmap procedures so their reply builders
    // run (info/stat/access/lookup/readlink/rename/commit and the NULL procs).
    void exercisesRemainingProcedures() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        { QFile f(dir.filePath("a.txt")); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("aaa"); }
        { QFile f(dir.filePath("b.txt")); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("bbb"); }
        NfsServer s;
        QVERIFY(s.start(dir.path(), 2049));

        // Root fh via MNT.
        XdrWriter mnt; mnt.str(dir.path());
        XdrReader mr(s.handleDatagram(rpcCall(1, 100005, 3, 1, mnt.data())));
        bool ok = false;
        for (int i = 0; i < 6; ++i) mr.u32(&ok);
        mr.u32(&ok);
        const QByteArray rootFh = mr.opaqueVar(&ok);
        QVERIFY(ok);

        auto call = [&](quint32 proc, const QByteArray& args) {
            const QByteArray r = s.handleDatagram(rpcCall(7, 100003, 3, proc, args));
            QVERIFY(!r.isEmpty());          // a reply was produced
        };
        auto fhArg = [&] { XdrWriter w; w.opaqueVar(rootFh); return w.data(); };

        // NULL procs.
        QVERIFY(!s.handleDatagram(rpcCall(1, 100000, 2, 0)).isEmpty());   // portmap NULL
        QVERIFY(!s.handleDatagram(rpcCall(1, 100005, 3, 0)).isEmpty());   // mount NULL
        QVERIFY(!s.handleDatagram(rpcCall(1, 100003, 3, 0)).isEmpty());   // nfs NULL

        call(1, fhArg());                    // GETATTR(root)
        call(19, fhArg());                   // FSINFO
        call(18, fhArg());                   // FSSTAT
        call(20, fhArg());                   // PATHCONF
        { XdrWriter w; w.opaqueVar(rootFh); w.u32(0x3F); call(4, w.data()); }   // ACCESS

        // LOOKUP a.txt → its fh, then GETATTR + READ it.
        XdrWriter lk; lk.opaqueVar(rootFh); lk.str(QStringLiteral("a.txt"));
        XdrReader lr(s.handleDatagram(rpcCall(7, 100003, 3, 3, lk.data())));
        for (int i = 0; i < 6; ++i) lr.u32(&ok);
        QCOMPARE(lr.u32(&ok), 0u);           // NFS3_OK
        const QByteArray aFh = lr.opaqueVar(&ok);
        QVERIFY(ok);
        { XdrWriter w; w.opaqueVar(aFh); w.u64(0); w.u32(3); call(6, w.data()); }   // READ
        { XdrWriter w; w.opaqueVar(aFh); call(5, w.data()); }                       // READLINK

        // RENAME a.txt → c.txt, COMMIT root.
        { XdrWriter w; w.opaqueVar(rootFh); w.str(QStringLiteral("a.txt"));
          w.opaqueVar(rootFh); w.str(QStringLiteral("c.txt")); call(14, w.data()); }
        { XdrWriter w; w.opaqueVar(rootFh); w.u64(0); w.u32(0); call(21, w.data()); } // COMMIT

        // LOOKUP of a missing name → NOENT (covers the failure branch).
        XdrWriter miss; miss.opaqueVar(rootFh); miss.str(QStringLiteral("nope"));
        QVERIFY(!s.handleDatagram(rpcCall(7, 100003, 3, 3, miss.data())).isEmpty());
        s.stop();
    }

    // Path-traversal + failure branches: a LOOKUP that escapes the export, a
    // WRITE to a bad handle, and REMOVE/RMDIR of missing entries.
    void rejectsEscapesAndBadOps() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        NfsServer s;
        QVERIFY(s.start(dir.path(), 2049));

        XdrWriter mnt; mnt.str(dir.path());
        XdrReader mr(s.handleDatagram(rpcCall(1, 100005, 3, 1, mnt.data())));
        bool ok = false;
        for (int i = 0; i < 6; ++i) mr.u32(&ok);
        mr.u32(&ok);
        const QByteArray rootFh = mr.opaqueVar(&ok);
        QVERIFY(ok);

        // LOOKUP "../../etc" must not resolve outside the export → NOENT.
        XdrWriter esc; esc.opaqueVar(rootFh); esc.str(QStringLiteral("../../etc"));
        XdrReader er(s.handleDatagram(rpcCall(7, 100003, 3, 3, esc.data())));
        for (int i = 0; i < 6; ++i) er.u32(&ok);
        QCOMPARE(er.u32(&ok), 2u);   // NFS3ERR_NOENT

        // REMOVE / RMDIR of a nonexistent name → error status, but a reply.
        { XdrWriter w; w.opaqueVar(rootFh); w.str(QStringLiteral("ghost"));
          QVERIFY(!s.handleDatagram(rpcCall(7, 100003, 3, 12, w.data())).isEmpty()); }
        { XdrWriter w; w.opaqueVar(rootFh); w.str(QStringLiteral("ghostdir"));
          QVERIFY(!s.handleDatagram(rpcCall(7, 100003, 3, 13, w.data())).isEmpty()); }

        // GETATTR on an unknown file handle → NOENT.
        { XdrWriter w; w.opaqueVar(QByteArray(32, '\xEE'));
          XdrReader gr(s.handleDatagram(rpcCall(7, 100003, 3, 1, w.data())));
          for (int i = 0; i < 6; ++i) gr.u32(&ok);
          QCOMPARE(gr.u32(&ok), 2u); }   // NOENT
        s.stop();
    }

    void malformedDatagramsIgnored() {
        NfsServer s;
        QTemporaryDir dir;
        QVERIFY(s.start(dir.path(), 2049));
        QVERIFY(s.handleDatagram(QByteArray()).isEmpty());       // empty
        QVERIFY(s.handleDatagram(QByteArray("\x00\x00\x00", 3)).isEmpty());  // truncated
        // A REPLY (msg_type 1) instead of a CALL (0) is not serviced.
        XdrWriter w; w.u32(9); w.u32(1); w.u32(2); w.u32(100003); w.u32(3); w.u32(1);
        QVERIFY(s.handleDatagram(w.data()).isEmpty());
        s.stop();
    }
};

QTEST_MAIN(TestNfs)
#include "test_nfs.moc"
