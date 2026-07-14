#include "core/IniStore.h"
#include <QtTest/QtTest>

using namespace macxterm::core;

class TestIni : public QObject {
    Q_OBJECT
private slots:
    void roundTrip() {
        SessionFolder root(QStringLiteral("Sessions"));
        Session web("web1", SessionType::Ssh);
        web.setHost("10.0.0.1"); web.setUsername("root"); web.setPort(22);
        root.addSession(web);
        SessionFolder* db = root.addFolder("Databases");
        Session pg("pg", SessionType::Ssh);
        pg.setHost("10.0.0.2"); pg.setUsername("admin");
        db->addSession(pg);

        const QByteArray blob = IniStore::serialize(root);
        SessionFolder loaded = IniStore::deserialize(blob);

        QCOMPARE(loaded.totalSessions(), 2);
        const Session* w = loaded.findSession("web1");
        QVERIFY(w != nullptr);
        QCOMPARE(w->host(), QStringLiteral("10.0.0.1"));
        QCOMPARE(w->username(), QStringLiteral("root"));
        const Session* p = loaded.findSession("pg");
        QVERIFY(p != nullptr);
        QCOMPARE(p->host(), QStringLiteral("10.0.0.2"));
    }

    void fileSaveLoad() {
        SessionFolder root(QStringLiteral("Sessions"));
        Session s("srv", SessionType::Telnet);
        s.setHost("host.example");
        root.addSession(s);
        const QString path = QDir::tempPath() + "/macxterm_test_sessions.ini";
        QVERIFY(IniStore::save(root, path));
        SessionFolder loaded;
        QVERIFY(IniStore::load(loaded, path));
        QCOMPARE(loaded.totalSessions(), 1);
        QVERIFY(loaded.findSession("srv") != nullptr);
        QFile::remove(path);
    }
};

QTEST_APPLESS_MAIN(TestIni)
#include "test_ini.moc"
