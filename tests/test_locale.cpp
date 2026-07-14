#include "connect/LocalShellConnection.h"
#include "core/Session.h"
#include <QtTest/QtTest>
#include <cstdlib>

using namespace macxterm;

// Regression test for CJK/multibyte garbling: when the app is launched without a
// locale in its environment (e.g. from Finder / as a .app bundle), the local
// shell must still run in a UTF-8 locale so tools emit valid UTF-8 rather than
// mangled multibyte output. LocalShellConnection sets LANG/LC_CTYPE to a UTF-8
// default (without overwriting an existing user locale). This test clears the
// locale in the parent and verifies the child shell reports a UTF-8 charmap.
class TestLocale : public QObject {
    Q_OBJECT
private slots:
    void childShellHasUtf8LocaleEvenWhenParentHasNone() {
#if defined(_WIN32)
        QSKIP("POSIX locale handling; Windows uses UTF-8 via ConPTY differently");
#else
        // Simulate a Finder / .app launch: no locale in the environment.
        ::unsetenv("LANG");
        ::unsetenv("LC_CTYPE");
        ::unsetenv("LC_ALL");

        connect::LocalShellConnection conn;
        core::Session s("local", core::SessionType::Shell);
        s.setParam("shell", "/bin/sh");
        QVERIFY(conn.connectSession(s));

        QByteArray got;
        connect(&conn, &connect::IConnection::dataReceived,
                [&](const QByteArray& d) { got += d; });
        conn.send("locale charmap\n");

        QTRY_VERIFY_WITH_TIMEOUT(got.toUpper().contains("UTF-8"), 4000);
        conn.disconnectSession();
#endif
    }
};

QTEST_GUILESS_MAIN(TestLocale)
#include "test_locale.moc"
