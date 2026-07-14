#include "connect/TelnetConnection.h"
#include "connect/TelnetProtocol.h"
#include "core/Session.h"
#include <QtTest/QtTest>
#include <QTcpServer>
#include <QTcpSocket>

using namespace macxterm;
using namespace macxterm::connect::telnet;

// End-to-end Telnet test against a mock server fixture: the server sends an IAC
// DO SGA negotiation plus a banner; TelnetConnection must negotiate (reply
// IAC WILL SGA) and surface the banner as clean application data.
class MockTelnetServer : public QObject {
    Q_OBJECT
public:
    bool listen() {
        connect(&m_server, &QTcpServer::newConnection, this, &MockTelnetServer::onConn);
        return m_server.listen(QHostAddress::LocalHost);
    }
    quint16 port() const { return m_server.serverPort(); }
    QByteArray received() const { return m_in; }
private slots:
    void onConn() {
        m_c = m_server.nextPendingConnection();
        connect(m_c, &QTcpSocket::readyRead, this, [this]{ m_in += m_c->readAll(); });
        QByteArray hello;
        hello.append(char(IAC)); hello.append(char(DO)); hello.append(char(OPT_SGA));
        hello.append("login: ");
        m_c->write(hello);
    }
private:
    QTcpServer m_server;
    QTcpSocket* m_c = nullptr;
    QByteArray m_in;
};

class TestTelnetLive : public QObject {
    Q_OBJECT
private slots:
    void negotiatesAndReceivesBanner() {
        MockTelnetServer srv;
        QVERIFY(srv.listen());

        connect::TelnetConnection conn;
        QByteArray appData;
        connect(&conn, &connect::IConnection::dataReceived, [&](const QByteArray& d){ appData += d; });

        core::Session s("t", core::SessionType::Telnet);
        s.setHost("127.0.0.1");
        s.setPort(srv.port());
        QVERIFY(conn.connectSession(s));

        // Banner arrives as clean app data (IAC stripped).
        QTRY_VERIFY_WITH_TIMEOUT(appData.contains("login: "), 3000);
        // Server should have received our IAC WILL SGA negotiation reply.
        QTRY_VERIFY_WITH_TIMEOUT(srv.received().size() >= 3, 3000);
        const QByteArray r = srv.received();
        QCOMPARE(static_cast<unsigned char>(r[0]), IAC);
        QCOMPARE(static_cast<unsigned char>(r[1]), WILL);
        QCOMPARE(static_cast<unsigned char>(r[2]), OPT_SGA);
        conn.disconnectSession();
    }
};

QTEST_GUILESS_MAIN(TestTelnetLive)
#include "test_telnet_live.moc"
