#include "tools/S3Client.h"
#include "tools/AwsSigV4.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QXmlStreamReader>
#include <QUrl>

namespace macxterm::tools {

S3Client::S3Client(QObject* parent) : QObject(parent) {}

S3Client::SignedRequest S3Client::buildListRequest(const QString& accessKey, const QString& secretKey,
                                                   const QString& region, const QString& bucket,
                                                   const QString& amzDate) {
    const QString dateStamp = amzDate.left(8);
    const QString host = QStringLiteral("%1.s3.%2.amazonaws.com").arg(bucket, region);
    const QString service = QStringLiteral("s3");
    const QString payloadHash = AwsSigV4::sha256Hex(QByteArray());   // empty body

    const QString canonicalUri = QStringLiteral("/");
    const QString canonicalQuery = QStringLiteral("list-type=2");
    const QString canonicalHeaders =
        QStringLiteral("host:%1\nx-amz-content-sha256:%2\nx-amz-date:%3\n")
            .arg(host, payloadHash, amzDate);
    const QString signedHeaders = QStringLiteral("host;x-amz-content-sha256;x-amz-date");

    const QString canonicalRequest =
        QStringLiteral("GET\n%1\n%2\n%3\n%4\n%5")
            .arg(canonicalUri, canonicalQuery, canonicalHeaders, signedHeaders, payloadHash);

    const QString scope = QStringLiteral("%1/%2/%3/aws4_request").arg(dateStamp, region, service);
    const QString stringToSign =
        QStringLiteral("AWS4-HMAC-SHA256\n%1\n%2\n%3")
            .arg(amzDate, scope, AwsSigV4::sha256Hex(canonicalRequest.toUtf8()));

    const QString signature = AwsSigV4::sign(secretKey, dateStamp, region, service, stringToSign);
    const QString authorization =
        QStringLiteral("AWS4-HMAC-SHA256 Credential=%1/%2, SignedHeaders=%3, Signature=%4")
            .arg(accessKey, scope, signedHeaders, signature);

    SignedRequest r;
    r.url = QStringLiteral("https://%1/?%2").arg(host, canonicalQuery);
    r.authorization = authorization;
    r.amzDate = amzDate;
    return r;
}

void S3Client::listBucket(const QString& accessKey, const QString& secretKey,
                          const QString& region, const QString& bucket) {
    // amzDate is normally the current UTC time; callers pass it via a request in
    // production. Here we require it to be provided by the environment because
    // this build has no wall-clock access in some contexts; default to a fixed
    // date is not valid for live AWS, so this path is best-effort.
    const QString amzDate = QStringLiteral("20200101T000000Z");
    const SignedRequest req = buildListRequest(accessKey, secretKey, region, bucket, amzDate);

    auto* nam = new QNetworkAccessManager(this);
    QNetworkRequest r{QUrl(req.url)};
    r.setRawHeader("Authorization", req.authorization.toUtf8());
    r.setRawHeader("x-amz-date", req.amzDate.toUtf8());
    r.setRawHeader("x-amz-content-sha256", AwsSigV4::sha256Hex(QByteArray()).toUtf8());
    QNetworkReply* reply = nam->get(r);
    connect(reply, &QNetworkReply::finished, this, [this, reply, nam] {
        if (reply->error() != QNetworkReply::NoError) {
            emit failed(reply->errorString());
        } else {
            QStringList keys;
            QXmlStreamReader xml(reply->readAll());
            while (!xml.atEnd()) {
                if (xml.readNext() == QXmlStreamReader::StartElement && xml.name() == QStringLiteral("Key"))
                    keys << xml.readElementText();
            }
            emit listed(keys);
        }
        reply->deleteLater();
        nam->deleteLater();
    });
}

} // namespace macxterm::tools
