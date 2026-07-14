#include "core/SshConfigImporter.h"
#include <QtTest/QtTest>

using namespace macxterm::core;

class TestSshConfig : public QObject {
    Q_OBJECT
private slots:
    void parsesHostBlocks() {
        const QByteArray cfg =
            "# my config\n"
            "Host web1\n"
            "    HostName 10.0.0.1\n"
            "    User root\n"
            "    Port 2222\n"
            "    IdentityFile ~/.ssh/id_web\n"
            "\n"
            "Host db\n"
            "    HostName db.internal\n"
            "    User admin\n"
            "    ProxyJump bastion\n";
        SessionFolder root = SshConfigImporter::parse(cfg);
        QCOMPARE(root.totalSessions(), 2);

        const Session* web = root.findSession("web1");
        QVERIFY(web != nullptr);
        QCOMPARE(web->type(), SessionType::Ssh);
        QCOMPARE(web->host(), QStringLiteral("10.0.0.1"));
        QCOMPARE(web->username(), QStringLiteral("root"));
        QCOMPARE(web->port(), 2222);
        QCOMPARE(web->param("keyfile"), QStringLiteral("~/.ssh/id_web"));

        const Session* db = root.findSession("db");
        QVERIFY(db != nullptr);
        QCOMPARE(db->param("jumphost"), QStringLiteral("bastion"));
    }

    void ignoresWildcardHosts() {
        const QByteArray cfg =
            "Host *\n"
            "    ForwardAgent yes\n"
            "Host real\n"
            "    HostName example.com\n";
        SessionFolder root = SshConfigImporter::parse(cfg);
        QCOMPARE(root.totalSessions(), 1);
        QVERIFY(root.findSession("real") != nullptr);
    }

    void hostNameDefaultsToAlias() {
        const QByteArray cfg = "Host solo\n    User me\n";
        SessionFolder root = SshConfigImporter::parse(cfg);
        const Session* s = root.findSession("solo");
        QVERIFY(s != nullptr);
        QCOMPARE(s->host(), QStringLiteral("solo"));  // falls back to alias
    }
};

QTEST_APPLESS_MAIN(TestSshConfig)
#include "test_sshconfig.moc"
