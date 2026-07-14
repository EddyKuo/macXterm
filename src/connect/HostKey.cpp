#include "connect/HostKey.h"
#include <QCryptographicHash>

namespace macxterm::connect {

QString HostKey::sha256Fingerprint(const QByteArray& keyBlob) {
    const QByteArray digest = QCryptographicHash::hash(keyBlob, QCryptographicHash::Sha256);
    // OpenSSH prints base64 without trailing '=' padding.
    QByteArray b64 = digest.toBase64(QByteArray::Base64Encoding | QByteArray::OmitTrailingEquals);
    return QStringLiteral("SHA256:") + QString::fromLatin1(b64);
}

} // namespace macxterm::connect
