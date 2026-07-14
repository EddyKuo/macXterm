#include "connect/HostKey.h"
#include <QtTest/QtTest>

using namespace macxterm::connect;

class TestHostKey : public QObject {
    Q_OBJECT
private slots:
    void fingerprintFormat() {
        const QString fp = HostKey::sha256Fingerprint(QByteArray("some-key-bytes"));
        QVERIFY(fp.startsWith("SHA256:"));
        QVERIFY(!fp.endsWith("="));           // padding omitted, OpenSSH-style
    }

    void deterministic() {
        const QByteArray blob("AAAAB3NzaC1lZDI1NTE5AAAAILVQ");
        QCOMPARE(HostKey::sha256Fingerprint(blob), HostKey::sha256Fingerprint(blob));
    }

    void differentKeysDifferentFingerprints() {
        QVERIFY(HostKey::sha256Fingerprint("keyA") != HostKey::sha256Fingerprint("keyB"));
    }

    void knownVector() {
        // SHA256 of empty input is well-known; base64 (no padding) of that digest.
        const QString fp = HostKey::sha256Fingerprint(QByteArray());
        QCOMPARE(fp, QStringLiteral("SHA256:47DEQpj8HBSa+/TImW+5JCeuQeRkm5NMpJWZG3hSuFU"));
    }
};

QTEST_APPLESS_MAIN(TestHostKey)
#include "test_hostkey.moc"
