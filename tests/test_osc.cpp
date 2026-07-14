#include "term/VtEngine.h"
#include <QtTest/QtTest>
#include <QSignalSpy>

using namespace macxterm::term;

// The OSC scanner in VtEngine extracts the shell's working directory (OSC 7)
// and window title (OSC 0/2) from the raw byte stream. Both are deterministic
// and testable without a PTY.
class TestOsc : public QObject {
    Q_OBJECT
private slots:
    void parsesOsc7Cwd() {
        VtEngine vt;
        QSignalSpy spy(&vt, &VtEngine::cwdChanged);
        // ESC ] 7 ; file://host/home/user/proj BEL
        vt.input(QByteArray("\x1b]7;file://myhost/home/user/proj\x07"));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("/home/user/proj"));
    }

    void parsesOsc7AcrossChunks() {
        VtEngine vt;
        QSignalSpy spy(&vt, &VtEngine::cwdChanged);
        // Split the sequence across two input() calls.
        vt.input(QByteArray("\x1b]7;file://h/var/l"));
        QCOMPARE(spy.count(), 0);
        vt.input(QByteArray("og\x07"));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("/var/log"));
    }

    void parsesOsc7StTerminator() {
        VtEngine vt;
        QSignalSpy spy(&vt, &VtEngine::cwdChanged);
        // Terminated by ST (ESC backslash) instead of BEL.
        vt.input(QByteArray("\x1b]7;file://h/tmp\x1b\\"));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("/tmp"));
    }

    void ignoresNonOsc7() {
        VtEngine vt;
        QSignalSpy spy(&vt, &VtEngine::cwdChanged);
        vt.input(QByteArray("plain text, no escapes"));
        vt.input(QByteArray("\x1b]0;just a title\x07"));   // OSC 0 = title, not cwd
        QCOMPARE(spy.count(), 0);
    }
};

QTEST_MAIN(TestOsc)
#include "test_osc.moc"
