#include "tools/SshExec.h"
#include "core/Session.h"
#include <QtTest/QtTest>
#include <cstdlib>

using namespace macxterm;

// Guarded-live test: run a command over SSH exec and read its output.
class TestSshExecLive : public QObject {
    Q_OBJECT
private slots:
    void runsRemoteCommand() {
        const char* host = std::getenv("MACXTERM_SSH_TEST_HOST");
        const char* user = std::getenv("MACXTERM_SSH_TEST_USER");
        if (!host || !user) QSKIP("Set MACXTERM_SSH_TEST_HOST/_USER to run the live SSH-exec test");

        tools::SshExec exec;
        core::Session s("x", core::SessionType::Ssh);
        s.setHost(host);
        s.setUsername(user);
        if (const char* pass = std::getenv("MACXTERM_SSH_TEST_PASS")) s.setParam("password", pass);
        if (const char* key = std::getenv("MACXTERM_SSH_TEST_KEY")) s.setParam("keyfile", key);
        if (const char* port = std::getenv("MACXTERM_SSH_TEST_PORT")) s.setPort(QString(port).toInt());

        QVERIFY(exec.connectSession(s));
        const QByteArray out = exec.run(QStringLiteral("echo MACXTERM_EXEC_OK"));
        QVERIFY2(out.contains("MACXTERM_EXEC_OK"), out.constData());
        exec.disconnectSession();
    }
};

QTEST_GUILESS_MAIN(TestSshExecLive)
#include "test_sshexec_live.moc"
