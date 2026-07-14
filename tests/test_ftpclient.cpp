#include "connect/FtpConnection.h"
#include "tools/FtpServer.h"
#include "core/Session.h"
#include <QtTest/QtTest>

using namespace macxterm;

// End-to-end FTP client test against the built-in FtpServer over loopback: the
// client connects, receives the 220 greeting, auto-sends USER, and the server
// replies 331. Exercises the real control-channel dialog both ways.
class TestFtpClient : public QObject {
    Q_OBJECT
private slots:
    void controlDialog() {
        tools::FtpServer server;
        QVERIFY(server.start(0));

        connect::FtpConnection conn;
        QByteArray got;
        connect(&conn, &connect::IConnection::dataReceived, [&](const QByteArray& d){ got += d; });

        core::Session s("ftp", core::SessionType::Ftp);
        s.setHost("127.0.0.1");
        s.setPort(server.port());
        s.setUsername("anonymous");
        QVERIFY(conn.connectSession(s));

        // Greeting arrives; the client auto-sends USER; server replies 331.
        QTRY_VERIFY_WITH_TIMEOUT(got.contains("220"), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(got.contains("331"), 3000);

        // User types a command; server replies.
        conn.send("PWD\r\n");
        QTRY_VERIFY_WITH_TIMEOUT(got.contains("257"), 3000);
        conn.disconnectSession();
    }
};

QTEST_GUILESS_MAIN(TestFtpClient)
#include "test_ftpclient.moc"
