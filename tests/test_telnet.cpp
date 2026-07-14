#include "connect/TelnetProtocol.h"
#include <QtTest/QtTest>

using namespace macxterm::connect;
using namespace macxterm::connect::telnet;

class TestTelnet : public QObject {
    Q_OBJECT
private slots:
    void stripsIacAndKeepsData() {
        TelnetProtocol p;
        QByteArray in;
        in.append("he");
        in.append(static_cast<char>(IAC)); in.append(static_cast<char>(DO)); in.append(static_cast<char>(OPT_TTYPE));
        in.append("llo");
        auto r = p.process(in);
        QCOMPARE(r.appData, QByteArray("hello"));
    }

    void refusesUnknownDoWithWont() {
        TelnetProtocol p;
        QByteArray in;
        in.append(static_cast<char>(IAC)); in.append(static_cast<char>(DO)); in.append(static_cast<char>(99));
        auto r = p.process(in);
        // Expect IAC WONT 99
        QCOMPARE(r.response.size(), 3);
        QCOMPARE(static_cast<unsigned char>(r.response[0]), IAC);
        QCOMPARE(static_cast<unsigned char>(r.response[1]), WONT);
        QCOMPARE(static_cast<unsigned char>(r.response[2]), (unsigned char)99);
    }

    void acceptsDoSgaWithWill() {
        TelnetProtocol p;
        QByteArray in;
        in.append(static_cast<char>(IAC)); in.append(static_cast<char>(DO)); in.append(static_cast<char>(OPT_SGA));
        auto r = p.process(in);
        QCOMPARE(static_cast<unsigned char>(r.response[1]), WILL);
    }

    void escapedFF() {
        TelnetProtocol p;
        QByteArray in;
        in.append('a');
        in.append(static_cast<char>(IAC)); in.append(static_cast<char>(IAC));  // literal 0xFF
        auto r = p.process(in);
        QCOMPARE(r.appData.size(), 2);
        QCOMPARE(static_cast<unsigned char>(r.appData[1]), (unsigned char)0xFF);
    }

    void iacSplitAcrossChunks() {
        TelnetProtocol p;
        QByteArray c1; c1.append('x'); c1.append(static_cast<char>(IAC));
        QByteArray c2; c2.append(static_cast<char>(DO)); c2.append(static_cast<char>(OPT_SGA));
        auto r1 = p.process(c1);
        QCOMPARE(r1.appData, QByteArray("x"));
        auto r2 = p.process(c2);
        QCOMPARE(static_cast<unsigned char>(r2.response[1]), WILL);   // negotiation completed across chunks
    }
};

QTEST_APPLESS_MAIN(TestTelnet)
#include "test_telnet.moc"
