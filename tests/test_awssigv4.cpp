#include "tools/AwsSigV4.h"
#include <QtTest/QtTest>

using namespace macxterm::tools;

// Verifies AWS Signature V4 against the values published in AWS documentation
// ("Examples of how to derive a signing key for Signature Version 4").
class TestAwsSigV4 : public QObject {
    Q_OBJECT
private slots:
    void signingKeyMatchesAwsDocExample() {
        // From the AWS docs derive-signing-key example.
        const QByteArray key = AwsSigV4::signingKey(
            QStringLiteral("wJalrXUtnFEMI/K7MDENG+bPxRfiCYEXAMPLEKEY"),
            QStringLiteral("20150830"), QStringLiteral("us-east-1"), QStringLiteral("iam"));
        QCOMPARE(QString::fromLatin1(key.toHex()),
                 QStringLiteral("c4afb1cc5771d871763a393e44b703571b55cc28424d1a5e86da6ed3c154a4b9"));
    }

    void sha256HexKnownVector() {
        // SHA-256 of the empty string.
        QCOMPARE(AwsSigV4::sha256Hex(QByteArray()),
                 QStringLiteral("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
    }

    void hmacIsDeterministic() {
        const QByteArray a = AwsSigV4::hmacSha256("key", "message");
        const QByteArray b = AwsSigV4::hmacSha256("key", "message");
        QCOMPARE(a, b);
        QVERIFY(!a.isEmpty());
    }
};

QTEST_APPLESS_MAIN(TestAwsSigV4)
#include "test_awssigv4.moc"
