#pragma once
#include <QObject>
#include <QString>
#include <QStringList>

namespace macxterm::tools {

// Minimal Amazon S3 client (S3 session type). Signs a ListObjectsV2 request with
// AWS SigV4 (tools::AwsSigV4) and fetches it over HTTPS — no AWS SDK dependency.
// Credentials come from the session (accessKey/secretKey/region/bucket). Live
// use needs real AWS credentials; the signing core is unit-tested separately.
class S3Client : public QObject {
    Q_OBJECT
public:
    explicit S3Client(QObject* parent = nullptr);

    // Build the canonical, signed request URL + Authorization header for a
    // ListObjectsV2 call. Exposed so the request construction is inspectable.
    struct SignedRequest { QString url; QString authorization; QString amzDate; };
    static SignedRequest buildListRequest(const QString& accessKey, const QString& secretKey,
                                          const QString& region, const QString& bucket,
                                          const QString& amzDate);   // amzDate: YYYYMMDDTHHMMSSZ

    // Fetch and parse the object keys of a bucket. Emits listed()/failed().
    void listBucket(const QString& accessKey, const QString& secretKey,
                    const QString& region, const QString& bucket);

signals:
    void listed(const QStringList& keys);
    void failed(const QString& message);
};

} // namespace macxterm::tools
