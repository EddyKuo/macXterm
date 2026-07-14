#include "tools/AwsSigV4.h"
#include <openssl/hmac.h>
#include <openssl/sha.h>

namespace macxterm::tools {

QByteArray AwsSigV4::hmacSha256(const QByteArray& key, const QByteArray& data) {
    unsigned char out[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    HMAC(EVP_sha256(),
         key.constData(), key.size(),
         reinterpret_cast<const unsigned char*>(data.constData()), data.size(),
         out, &len);
    return QByteArray(reinterpret_cast<char*>(out), static_cast<int>(len));
}

QString AwsSigV4::sha256Hex(const QByteArray& data) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(data.constData()), data.size(), digest);
    return QByteArray(reinterpret_cast<char*>(digest), SHA256_DIGEST_LENGTH).toHex();
}

QByteArray AwsSigV4::signingKey(const QString& secretKey, const QString& dateStamp,
                                const QString& region, const QString& service) {
    const QByteArray kSecret = ("AWS4" + secretKey).toUtf8();
    const QByteArray kDate    = hmacSha256(kSecret, dateStamp.toUtf8());
    const QByteArray kRegion  = hmacSha256(kDate, region.toUtf8());
    const QByteArray kService = hmacSha256(kRegion, service.toUtf8());
    return hmacSha256(kService, QByteArray("aws4_request"));
}

QString AwsSigV4::sign(const QString& secretKey, const QString& dateStamp,
                       const QString& region, const QString& service,
                       const QString& stringToSign) {
    const QByteArray key = signingKey(secretKey, dateStamp, region, service);
    return QString::fromLatin1(hmacSha256(key, stringToSign.toUtf8()).toHex());
}

} // namespace macxterm::tools
