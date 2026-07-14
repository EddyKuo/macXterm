#include "tools/HttpServer.h"
#include <QtTest/QtTest>
#include <QTcpSocket>
#include <QTemporaryDir>

using namespace macxterm::tools;

class TestHttpServer : public QObject {
    Q_OBJECT
private slots:
    void parsesRequestPath() {
        QCOMPARE(HttpServer::parseRequestPath("GET /index.html HTTP/1.1"),
                 QStringLiteral("/index.html"));
        QCOMPARE(HttpServer::parseRequestPath("GET /a?x=1 HTTP/1.1"), QStringLiteral("/a"));
        QVERIFY(HttpServer::parseRequestPath("POST /x HTTP/1.1").isEmpty());  // only GET
    }

    void servesFileOverLoopback() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile f(dir.filePath("hello.txt"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("MACXTERM_HTTP_OK");
        f.close();

        HttpServer server;
        QVERIFY(server.start(dir.path(), 0));
        QVERIFY(server.isRunning());

        QTcpSocket c;
        QByteArray resp;
        connect(&c, &QTcpSocket::readyRead, [&] { resp += c.readAll(); });
        c.connectToHost(QHostAddress::LocalHost, server.port());
        QVERIFY(c.waitForConnected(2000));
        c.write("GET /hello.txt HTTP/1.1\r\nHost: x\r\n\r\n");
        QTRY_VERIFY_WITH_TIMEOUT(resp.contains("MACXTERM_HTTP_OK"), 4000);
        QVERIFY2(resp.contains("200 OK"), resp.constData());
    }

    void missingFileGives404() {
        QTemporaryDir dir;
        HttpServer server;
        QVERIFY(server.start(dir.path(), 0));
        QTcpSocket c;
        QByteArray resp;
        connect(&c, &QTcpSocket::readyRead, [&] { resp += c.readAll(); });
        c.connectToHost(QHostAddress::LocalHost, server.port());
        QVERIFY(c.waitForConnected(2000));
        c.write("GET /nope.txt HTTP/1.1\r\n\r\n");
        QTRY_VERIFY_WITH_TIMEOUT(resp.contains("404"), 4000);
    }
};

QTEST_GUILESS_MAIN(TestHttpServer)
#include "test_httpserver.moc"
