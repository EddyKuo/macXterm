#include "tools/TelnetServer.h"
#include <QtTest/QtTest>
#include <QTcpSocket>

using namespace macxterm::tools;

// The embedded Telnet server spawns a login shell per connection over a PTY.
// Connect a client, run a command, and confirm the shell's output comes back.
class TestTelnetServer : public QObject {
    Q_OBJECT
private slots:
    void startStopAndPort() {
        TelnetServer s;
        QVERIFY(s.start(0));                 // ephemeral port
        QVERIFY(s.isRunning());
        QVERIFY(s.port() != 0);
        s.stop();
        QVERIFY(!s.isRunning());
    }

    void shellEchoesCommand() {
        TelnetServer s;
        QVERIFY(s.start(0));
        const quint16 port = s.port();

        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, port);
        QVERIFY(client.waitForConnected(3000));

        QByteArray got;
        connect(&client, &QTcpSocket::readyRead, [&] { got += client.readAll(); });

        // Run a command whose output we can recognize.
        client.write("echo TELNET_OK\n");
        client.flush();
        QTRY_VERIFY_WITH_TIMEOUT(got.contains("TELNET_OK"), 6000);

        client.disconnectFromHost();
        s.stop();
    }

    void secondStartRebinds() {
        TelnetServer s;
        QVERIFY(s.start(0));
        s.stop();
        QVERIFY(s.start(0));                 // starting again must succeed
        QVERIFY(s.isRunning());
        s.stop();
    }
};

QTEST_MAIN(TestTelnetServer)
#include "test_telnetserver.moc"
