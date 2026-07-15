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

    // ── Advanced per-protocol option serialization (SessionForm::applyAdvanced) ──

    void advancedSshMinimalDefaults() {
        // All-default SSH options → nothing serialized (defaults are implicit).
        QVariantMap f;
        SessionForm::AdvancedOptions o;   // x11 default on, rest off
        SessionForm::applyAdvanced(f, SessionType::Ssh, false, o);
        QVERIFY(!f.contains("compression"));
        QVERIFY(!f.contains("x11"));       // default-on → omitted when enabled
        QVERIFY(!f.contains("agent"));
    }

    void advancedSshFlags() {
        QVariantMap f;
        SessionForm::AdvancedOptions o;
        o.compression = true; o.x11 = false; o.agent = true; o.agentForward = true;
        SessionForm::applyAdvanced(f, SessionType::Ssh, false, o);
        QCOMPARE(f.value("compression").toString(), QStringLiteral("1"));
        QCOMPARE(f.value("x11").toString(), QStringLiteral("0"));   // off → serialized
        QCOMPARE(f.value("agent").toString(), QStringLiteral("1"));
        QCOMPARE(f.value("agentforward").toString(), QStringLiteral("1"));
    }

    void advancedSshKeepaliveRemoteCmdStayOpen() {
        QVariantMap f;
        SessionForm::AdvancedOptions o;
        o.sshKeepalive = 30;
        o.sshRemoteCommand = "tail -f /var/log/syslog";
        o.sshStayOpen = true;
        SessionForm::applyAdvanced(f, SessionType::Ssh, false, o);
        QCOMPARE(f.value("keepalive").toString(), QStringLiteral("30"));
        QCOMPARE(f.value("remotecommand").toString(), QStringLiteral("tail -f /var/log/syslog"));
        QCOMPARE(f.value("stayopen").toString(), QStringLiteral("1"));
    }

    void advancedSshKeepaliveOffOmitted() {
        QVariantMap f;
        SessionForm::AdvancedOptions o;   // sshKeepalive = 0, no command, not stay-open
        SessionForm::applyAdvanced(f, SessionType::Ssh, false, o);
        QVERIFY(!f.contains("keepalive"));       // 0 = off → omitted
        QVERIFY(!f.contains("remotecommand"));
        QVERIFY(!f.contains("stayopen"));
    }

    void advancedGatewayCredsOnlyWhenGateway() {
        SessionForm::AdvancedOptions o;
        o.gatewayUser = "jump"; o.gatewayPassword = "pw";
        QVariantMap without;
        SessionForm::applyAdvanced(without, SessionType::Ssh, /*hasGateway=*/false, o);
        QVERIFY(!without.contains("gateway_user"));
        QVariantMap with;
        SessionForm::applyAdvanced(with, SessionType::Ssh, /*hasGateway=*/true, o);
        QCOMPARE(with.value("gateway_user").toString(), QStringLiteral("jump"));
        QCOMPARE(with.value("gateway_password").toString(), QStringLiteral("pw"));
    }

    void advancedRdpOptions() {
        QVariantMap f;
        SessionForm::AdvancedOptions o;
        o.domain = "CORP"; o.rdpWidth = 1920; o.rdpHeight = 1080;
        o.rdpClipboard = false; o.rdpDrives = true; o.rdpNla = false; o.rdpIgnoreCert = true;
        SessionForm::applyAdvanced(f, SessionType::Rdp, false, o);
        QCOMPARE(f.value("domain").toString(), QStringLiteral("CORP"));
        QCOMPARE(f.value("width").toString(), QStringLiteral("1920"));
        QCOMPARE(f.value("height").toString(), QStringLiteral("1080"));
        QCOMPARE(f.value("redirect_clipboard").toString(), QStringLiteral("0"));  // default-on → off
        QCOMPARE(f.value("redirect_drives").toString(), QStringLiteral("1"));
        QCOMPARE(f.value("nla").toString(), QStringLiteral("0"));
        QCOMPARE(f.value("ignorecert").toString(), QStringLiteral("1"));
    }

    void advancedRdpDefaultResolutionOmitted() {
        QVariantMap f;
        SessionForm::AdvancedOptions o;   // rdpWidth/Height = 0
        SessionForm::applyAdvanced(f, SessionType::Rdp, false, o);
        QVERIFY(!f.contains("width"));
        QVERIFY(!f.contains("height"));
    }

    void advancedVncViewOnly() {
        QVariantMap f;
        SessionForm::AdvancedOptions o; o.vncViewOnly = true;
        SessionForm::applyAdvanced(f, SessionType::Vnc, false, o);
        QCOMPARE(f.value("viewonly").toString(), QStringLiteral("1"));
    }

    void advancedIgnoresWrongTypeParams() {
        // SSH options must not leak onto an RDP session.
        QVariantMap f;
        SessionForm::AdvancedOptions o; o.compression = true; o.rdpDrives = true;
        SessionForm::applyAdvanced(f, SessionType::Rdp, false, o);
        QVERIFY(!f.contains("compression"));            // SSH-only, skipped
        QCOMPARE(f.value("redirect_drives").toString(), QStringLiteral("1"));
    }
};

QTEST_APPLESS_MAIN(TestSessionForm)
#include "test_sessionform.moc"
