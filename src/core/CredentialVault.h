#pragma once
#include <QString>
#include <QByteArray>
#include <QMap>

namespace macxterm::core {

// Cross-platform encrypted credential store (ADR_6 / Architecture §10):
//   - key derived from a master password via Argon2id
//   - secrets encrypted with AES-256-GCM (authenticated)
//   - never persists plaintext; auth-tag verified on decrypt (tamper-evident)
//
// On-disk blob layout (all lengths fixed except ciphertext):
//   magic[8] "MXVAULT1" | salt[16] | nonce[12] | tag[16] | ciphertext[...]
// where plaintext is a UTF-8 serialization of the key/value secret map.
class CredentialVault {
public:
    // Set/replace a secret (e.g. per-session password or key passphrase).
    void setSecret(const QString& id, const QString& secret) { m_secrets.insert(id, secret); }
    QString secret(const QString& id) const { return m_secrets.value(id); }
    bool hasSecret(const QString& id) const { return m_secrets.contains(id); }
    void removeSecret(const QString& id) { m_secrets.remove(id); }
    int count() const { return m_secrets.size(); }
    void clear() { m_secrets.clear(); }

    // Encrypt the current secret map under `masterPassword`. Returns the blob.
    QByteArray encrypt(const QString& masterPassword) const;

    // Decrypt `blob` under `masterPassword`, replacing the in-memory map.
    // Returns false on wrong password / tampering / malformed blob.
    bool decrypt(const QByteArray& blob, const QString& masterPassword);

    // File helpers.
    bool save(const QString& path, const QString& masterPassword) const;
    bool load(const QString& path, const QString& masterPassword);

private:
    QMap<QString, QString> m_secrets;
};

} // namespace macxterm::core
