#include "tools/S3Client.h"
#include <QtTest/QtTest>

using namespace macxterm::tools;

// The S3 ListObjectsV2 request builder is deterministic given fixed inputs, so
// its signed URL/header structure is unit-testable without hitting AWS.
class TestS3 : public QObject {
    Q_OBJECT
private slots:
    void buildsSignedListRequest() {
        const auto req = S3Client::buildListRequest(
            QStringLiteral("AKIDEXAMPLE"),
            QStringLiteral("wJalrXUtnFEMI/K7MDENG+bPxRfiCYEXAMPLEKEY"),
            QStringLiteral("us-east-1"), QStringLiteral("my-bucket"),
            QStringLiteral("20200101T000000Z"));

        QCOMPARE(req.url, QStringLiteral("https://my-bucket.s3.us-east-1.amazonaws.com/?list-type=2"));
        QVERIFY(req.authorization.startsWith("AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20200101/us-east-1/s3/aws4_request"));
        QVERIFY(req.authorization.contains("SignedHeaders=host;x-amz-content-sha256;x-amz-date"));
        QVERIFY(req.authorization.contains("Signature="));
        // Signature is a 64-char lowercase hex string.
        const QString sig = req.authorization.section("Signature=", 1, 1);
        QCOMPARE(sig.size(), 64);
    }

    void signatureIsDeterministic() {
        auto a = S3Client::buildListRequest("k", "s", "us-east-1", "b", "20200101T000000Z");
        auto b = S3Client::buildListRequest("k", "s", "us-east-1", "b", "20200101T000000Z");
        QCOMPARE(a.authorization, b.authorization);
    }
};

QTEST_APPLESS_MAIN(TestS3)
#include "test_s3.moc"
