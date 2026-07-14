#include "connect/VncConnection.h"
#include "connect/RfbProtocol.h"
#include "core/Session.h"
#include <QtTest/QtTest>
#include <QTcpServer>
#include <QTcpSocket>

using namespace macxterm;

// A minimal mock RFB 3.8 server: performs the handshake and pushes one RAW
// framebuffer update, so the real VncConnection is exercised end-to-end over
// loopback (no external VNC server needed).
class MockRfbServer : public QObject {
    Q_OBJECT
public:
    bool listen() {
        connect(&m_server, &QTcpServer::newConnection, this, &MockRfbServer::onConn);
        return m_server.listen(QHostAddress::LocalHost);
    }
    quint16 port() const { return m_server.serverPort(); }
private slots:
    void onConn() {
        m_c = m_server.nextPendingConnection();
        connect(m_c, &QTcpSocket::readyRead, this, &MockRfbServer::onData);
        m_c->write("RFB 003.008\n");           // ProtocolVersion
        m_step = 1;
    }
    void onData() {
        m_in += m_c->readAll();
        if (m_step == 1 && m_in.size() >= 12) {  // got client version
            m_in.clear();
            QByteArray sec; sec.append(char(1)); sec.append(char(1));  // 1 type: None
            m_c->write(sec);
            m_step = 2;
        } else if (m_step == 2 && m_in.size() >= 1) {  // got security choice
            m_in.clear();
            m_c->write(QByteArray(4, char(0)));   // SecurityResult OK
            m_step = 3;
        } else if (m_step == 3 && m_in.size() >= 1) {  // got ClientInit
            m_in.clear();
            QByteArray si;
            auto p16 = [&](int v){ si.append(char((v>>8)&0xff)); si.append(char(v&0xff)); };
            auto p32 = [&](int v){ si.append(char((v>>24)&0xff)); si.append(char((v>>16)&0xff));
                                   si.append(char((v>>8)&0xff)); si.append(char(v&0xff)); };
            p16(2); p16(1);            // 2x1 framebuffer
            si.append(QByteArray(16, char(0)));   // pixel format
            p32(4); si.append("demo");            // name
            m_c->write(si);
            m_step = 4;
        } else if (m_step == 4 && m_in.size() >= 1) {  // got FBUpdateRequest
            QByteArray fb;
            fb.append(char(0)); fb.append(char(0));   // msg-type, padding
            fb.append(char(0)); fb.append(char(1));   // 1 rectangle
            auto p16 = [&](int v){ fb.append(char((v>>8)&0xff)); fb.append(char(v&0xff)); };
            p16(0); p16(0); p16(2); p16(1);           // x,y,w,h
            fb.append(QByteArray(4, char(0)));        // encoding 0 (RAW)
            // 2 BGRA pixels: red, green
            fb.append(char(0)); fb.append(char(0)); fb.append(char(0xFF)); fb.append(char(0xFF));
            fb.append(char(0)); fb.append(char(0xFF)); fb.append(char(0)); fb.append(char(0xFF));
            m_c->write(fb);
            m_step = 5;
        }
    }
private:
    QTcpServer m_server;
    QTcpSocket* m_c = nullptr;
    QByteArray m_in;
    int m_step = 0;
};

class TestVnc : public QObject {
    Q_OBJECT
private slots:
    void handshakeAndFramebufferOverLoopback() {
        MockRfbServer srv;
        QVERIFY(srv.listen());

        connect::VncConnection vnc;
        int gotW = 0, gotH = 0;
        QList<quint32> gotPixels;
        connect(&vnc, &connect::VncConnection::serverReady, [&](int w, int h, const QString&) { gotW = w; gotH = h; });
        connect(&vnc, &connect::VncConnection::rectDecoded,
                [&](int, int, int, int, const QList<quint32>& px) { gotPixels = px; });

        core::Session s("v", core::SessionType::Vnc);
        s.setHost("127.0.0.1");
        s.setPort(srv.port());
        QVERIFY(vnc.connectSession(s));

        QTRY_COMPARE_WITH_TIMEOUT(vnc.state(), connect::IConnection::State::Connected, 4000);
        QCOMPARE(gotW, 2);
        QCOMPARE(gotH, 1);
        QTRY_VERIFY_WITH_TIMEOUT(gotPixels.size() == 2, 4000);
        QCOMPARE(gotPixels[0], quint32(0xFFFF0000));   // red
        QCOMPARE(gotPixels[1], quint32(0xFF00FF00));   // green
    }
};

QTEST_GUILESS_MAIN(TestVnc)
#include "test_vnc.moc"
