#include "core/Store.h"
#include <QtTest/QtTest>

using namespace macxterm::core;

class TestStore : public QObject {
    Q_OBJECT
private slots:
    void opensInMemory() {
        Store s;
        QVERIFY(s.open(":memory:"));
        QCOMPARE(s.schemaVersion(), 1);
    }

    void saveAndLoadTree() {
        Store s;
        QVERIFY(s.open(":memory:"));

        SessionFolder root(QStringLiteral("Sessions"));
        Session web("web1", SessionType::Ssh);
        web.setHost("10.0.0.1"); web.setUsername("root"); web.setPort(2222);
        web.setParam("keyfile", "~/.ssh/id");
        root.addSession(web);
        SessionFolder* db = root.addFolder("DB");
        Session pg("pg", SessionType::Ssh);
        pg.setHost("10.0.0.2");
        db->addSession(pg);

        QVERIFY(s.saveTree(root));
        SessionFolder loaded = s.loadTree();
        QCOMPARE(loaded.totalSessions(), 2);

        const Session* w = loaded.findSession("web1");
        QVERIFY(w != nullptr);
        QCOMPARE(w->host(), QStringLiteral("10.0.0.1"));
        QCOMPARE(w->port(), 2222);
        QCOMPARE(w->param("keyfile"), QStringLiteral("~/.ssh/id"));
        QVERIFY(loaded.findSession("pg") != nullptr);
    }

    void saveTreeReplacesOldRows() {
        Store s;
        QVERIFY(s.open(":memory:"));
        SessionFolder r1(QStringLiteral("Sessions"));
        r1.addSession(Session("a", SessionType::Ssh));
        QVERIFY(s.saveTree(r1));
        SessionFolder r2(QStringLiteral("Sessions"));
        r2.addSession(Session("b", SessionType::Telnet));
        QVERIFY(s.saveTree(r2));
        SessionFolder loaded = s.loadTree();
        QCOMPARE(loaded.totalSessions(), 1);
        QVERIFY(loaded.findSession("b") != nullptr);
        QVERIFY(loaded.findSession("a") == nullptr);
    }

    void knownHostsPinning() {
        Store s;
        QVERIFY(s.open(":memory:"));
        QVERIFY(s.upsertKnownHost("host.example", 22, "ssh-ed25519", "SHA256:abc"));
        QCOMPARE(s.knownHostFingerprint("host.example", 22, "ssh-ed25519"),
                 QStringLiteral("SHA256:abc"));
        // Update in place.
        QVERIFY(s.upsertKnownHost("host.example", 22, "ssh-ed25519", "SHA256:xyz"));
        QCOMPARE(s.knownHostFingerprint("host.example", 22, "ssh-ed25519"),
                 QStringLiteral("SHA256:xyz"));
        // Unknown host → empty.
        QVERIFY(s.knownHostFingerprint("other", 22, "ssh-rsa").isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestStore)
#include "test_store.moc"
