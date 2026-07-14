#include "core/SessionForm.h"
#include <QtTest/QtTest>

using namespace macxterm::core;

class TestSessionForm : public QObject {
    Q_OBJECT
private slots:
    void toSessionMapsCoreFields() {
        QVariantMap f;
        f.insert("name", "web1");
        f.insert("type", "SSH");
        f.insert("host", "10.0.0.1");
        f.insert("port", "2222");
        f.insert("username", "root");
        f.insert("keyfile", "~/.ssh/id");   // extra param passthrough
        Session s = SessionForm::toSession(f);
        QCOMPARE(s.name(), QStringLiteral("web1"));
        QCOMPARE(s.type(), SessionType::Ssh);
        QCOMPARE(s.host(), QStringLiteral("10.0.0.1"));
        QCOMPARE(s.port(), 2222);
        QCOMPARE(s.username(), QStringLiteral("root"));
        QCOMPARE(s.param("keyfile"), QStringLiteral("~/.ssh/id"));
    }

    void roundTripThroughForm() {
        Session s("db", SessionType::Telnet);
        s.setHost("h"); s.setUsername("u");
        QVariantMap f = SessionForm::fromSession(s);
        Session back = SessionForm::toSession(f);
        QVERIFY(back == s);
    }

    void validateRequiresName() {
        QVariantMap f; f.insert("type", "SSH"); f.insert("host", "h");
        QVERIFY(!SessionForm::validate(f).isEmpty());
    }

    void validateRequiresHostForNetworkTypes() {
        QVariantMap f; f.insert("name", "x"); f.insert("type", "SSH");
        QVERIFY(!SessionForm::validate(f).isEmpty());   // no host
        f.insert("host", "h");
        QVERIFY(SessionForm::validate(f).isEmpty());    // now ok
    }

    void validateShellNeedsNoHost() {
        QVariantMap f; f.insert("name", "local"); f.insert("type", "Shell");
        QVERIFY(SessionForm::validate(f).isEmpty());
    }
};

QTEST_APPLESS_MAIN(TestSessionForm)
#include "test_sessionform.moc"
