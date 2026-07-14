#pragma once
#include <QByteArray>
#include <QString>

namespace macxterm::tools {

// AWS Signature Version 4 signing (used by the S3 session type). No AWS SDK
// dependency — just HMAC-SHA256 (OpenSSL). The signing-key derivation is
// verifiable against AWS's published documentation example (see test_awssigv4).
class AwsSigV4 {
public:
    // Derive the SigV4 signing key: HMAC chain over date/region/service.
    static QByteArray signingKey(const QString& secretKey, const QString& dateStamp,
                                 const QString& region, const QString& service);

    // HMAC-SHA256(key, data).
    static QByteArray hmacSha256(const QByteArray& key, const QByteArray& data);
    // Lowercase hex SHA-256 of data.
    static QString sha256Hex(const QByteArray& data);

    // Compute the final signature (lowercase hex) for a string-to-sign.
    static QString sign(const QString& secretKey, const QString& dateStamp,
                        const QString& region, const QString& service,
                        const QString& stringToSign);
};

} // namespace macxterm::tools
