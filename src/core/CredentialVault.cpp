#include "core/CredentialVault.h"
#include <QFile>
#include <QStringList>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/kdf.h>
#include <openssl/core_names.h>
#include <openssl/params.h>
#include <cstring>
#include <cstdint>

namespace macxterm::core {

namespace {
constexpr char kMagic[8] = {'M','X','V','A','U','L','T','1'};
constexpr int  kSaltLen  = 16;
constexpr int  kNonceLen = 12;
constexpr int  kTagLen   = 16;
constexpr int  kKeyLen   = 32; // AES-256

// Argon2id parameters (interactive-ish; documented in Architecture §10).
constexpr uint32_t kArgonTime = 3;
constexpr uint32_t kArgonMemKiB = 64 * 1024; // 64 MiB
constexpr uint32_t kArgonLanes = 1;

// Serialize the secret map to a UTF-8 plaintext blob (\n-separated "id\tsecret").
QByteArray flatten(const QMap<QString, QString>& m) {
    QByteArray out;
    for (auto it = m.constBegin(); it != m.constEnd(); ++it) {
        out += it.key().toUtf8();
        out += '\t';
        out += it.value().toUtf8();
        out += '\n';
    }
    return out;
}

void unflatten(const QByteArray& data, QMap<QString, QString>& m) {
    m.clear();
    const QStringList lines = QString::fromUtf8(data).split('\n', Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        const int tab = line.indexOf('\t');
        if (tab > 0) m.insert(line.left(tab), line.mid(tab + 1));
    }
}

// KDF identifiers stored in the blob so a vault decrypts correctly regardless of
// which KDF the writing build had available (portability — profile §3 allows
// "Argon2id/scrypt"). Argon2 needs OpenSSL 3.2+; older OpenSSL (e.g. Ubuntu
// 24.04's 3.0) falls back to scrypt, which is available since OpenSSL 3.0.
enum Kdf : unsigned char { KdfArgon2id = 1, KdfScrypt = 2 };

// Preferred KDF for *new* vaults on this build.
constexpr unsigned char preferredKdf() {
#ifdef OSSL_KDF_PARAM_ARGON2_MEMCOST
    return KdfArgon2id;
#else
    return KdfScrypt;
#endif
}

bool deriveArgon2id(const QByteArray& pw, const QByteArray& salt, QByteArray& key) {
#ifdef OSSL_KDF_PARAM_ARGON2_MEMCOST
    EVP_KDF* kdf = EVP_KDF_fetch(nullptr, "ARGON2ID", nullptr);
    if (!kdf) return false;
    EVP_KDF_CTX* kctx = EVP_KDF_CTX_new(kdf);
    EVP_KDF_free(kdf);
    if (!kctx) return false;
    uint32_t iter = kArgonTime, lanes = kArgonLanes, threads = 1, memcost = kArgonMemKiB;
    OSSL_PARAM p[7], *q = p;
    *q++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_PASSWORD, const_cast<char*>(pw.constData()), pw.size());
    *q++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT, const_cast<char*>(salt.constData()), salt.size());
    *q++ = OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ITER, &iter);
    *q++ = OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_THREADS, &threads);
    *q++ = OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ARGON2_LANES, &lanes);
    *q++ = OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ARGON2_MEMCOST, &memcost);
    *q = OSSL_PARAM_construct_end();
    const int rc = EVP_KDF_derive(kctx, reinterpret_cast<unsigned char*>(key.data()), kKeyLen, p);
    EVP_KDF_CTX_free(kctx);
    return rc == 1;
#else
    (void)pw; (void)salt; (void)key;
    return false;
#endif
}

bool deriveScrypt(const QByteArray& pw, const QByteArray& salt, QByteArray& key) {
    EVP_KDF* kdf = EVP_KDF_fetch(nullptr, "SCRYPT", nullptr);
    if (!kdf) return false;
    EVP_KDF_CTX* kctx = EVP_KDF_CTX_new(kdf);
    EVP_KDF_free(kdf);
    if (!kctx) return false;
    uint64_t n = 16384; uint32_t r = 8, p32 = 1;
    OSSL_PARAM p[6], *q = p;
    *q++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_PASSWORD, const_cast<char*>(pw.constData()), pw.size());
    *q++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT, const_cast<char*>(salt.constData()), salt.size());
    *q++ = OSSL_PARAM_construct_uint64(OSSL_KDF_PARAM_SCRYPT_N, &n);
    *q++ = OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_SCRYPT_R, &r);
    *q++ = OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_SCRYPT_P, &p32);
    *q = OSSL_PARAM_construct_end();
    const int rc = EVP_KDF_derive(kctx, reinterpret_cast<unsigned char*>(key.data()), kKeyLen, p);
    EVP_KDF_CTX_free(kctx);
    return rc == 1;
}

// Derive a 32-byte key using the given KDF id (ADR_6 / §10).
bool deriveKey(const QString& password, const QByteArray& salt, unsigned char kdfId, QByteArray& key) {
    key.resize(kKeyLen);
    const QByteArray pw = password.toUtf8();
    if (kdfId == KdfArgon2id) return deriveArgon2id(pw, salt, key);
    if (kdfId == KdfScrypt)   return deriveScrypt(pw, salt, key);
    return false;
}
} // namespace

QByteArray CredentialVault::encrypt(const QString& masterPassword) const {
    QByteArray salt(kSaltLen, 0), nonce(kNonceLen, 0);
    if (RAND_bytes(reinterpret_cast<unsigned char*>(salt.data()), kSaltLen) != 1) return {};
    if (RAND_bytes(reinterpret_cast<unsigned char*>(nonce.data()), kNonceLen) != 1) return {};

    const unsigned char kdfId = preferredKdf();
    QByteArray key;
    if (!deriveKey(masterPassword, salt, kdfId, key)) return {};

    const QByteArray plain = flatten(m_secrets);
    QByteArray cipher(plain.size(), 0);
    QByteArray tag(kTagLen, 0);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};
    int len = 0, ok = 1;
    ok &= EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    ok &= EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kNonceLen, nullptr);
    ok &= EVP_EncryptInit_ex(ctx, nullptr, nullptr,
                             reinterpret_cast<const unsigned char*>(key.constData()),
                             reinterpret_cast<const unsigned char*>(nonce.constData()));
    if (!plain.isEmpty()) {
        ok &= EVP_EncryptUpdate(ctx,
                                reinterpret_cast<unsigned char*>(cipher.data()), &len,
                                reinterpret_cast<const unsigned char*>(plain.constData()),
                                plain.size());
    }
    int finalLen = 0;
    ok &= EVP_EncryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(cipher.data()) + len, &finalLen);
    ok &= EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, kTagLen, tag.data());
    EVP_CIPHER_CTX_free(ctx);
    OPENSSL_cleanse(key.data(), key.size());   // wipe derived key (§10)
    if (!ok) return {};

    QByteArray blob;
    blob.append(kMagic, sizeof(kMagic));
    blob.append(static_cast<char>(kdfId));   // KDF marker for portable decrypt
    blob.append(salt);
    blob.append(nonce);
    blob.append(tag);
    blob.append(cipher);
    return blob;
}

bool CredentialVault::decrypt(const QByteArray& blob, const QString& masterPassword) {
    const int header = sizeof(kMagic) + 1 + kSaltLen + kNonceLen + kTagLen;
    if (blob.size() < header) return false;
    if (std::memcmp(blob.constData(), kMagic, sizeof(kMagic)) != 0) return false;

    int off = sizeof(kMagic);
    const unsigned char kdfId = static_cast<unsigned char>(blob[off]); off += 1;
    const QByteArray salt  = blob.mid(off, kSaltLen);  off += kSaltLen;
    const QByteArray nonce = blob.mid(off, kNonceLen); off += kNonceLen;
    QByteArray tag         = blob.mid(off, kTagLen);   off += kTagLen;
    const QByteArray cipher = blob.mid(off);

    QByteArray key;
    if (!deriveKey(masterPassword, salt, kdfId, key)) return false;

    QByteArray plain(cipher.size(), 0);
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return false;
    int len = 0, ok = 1;
    ok &= EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    ok &= EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kNonceLen, nullptr);
    ok &= EVP_DecryptInit_ex(ctx, nullptr, nullptr,
                             reinterpret_cast<const unsigned char*>(key.constData()),
                             reinterpret_cast<const unsigned char*>(nonce.constData()));
    if (!cipher.isEmpty()) {
        ok &= EVP_DecryptUpdate(ctx,
                                reinterpret_cast<unsigned char*>(plain.data()), &len,
                                reinterpret_cast<const unsigned char*>(cipher.constData()),
                                cipher.size());
    }
    ok &= EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, kTagLen, tag.data());
    int finalLen = 0;
    // EVP_DecryptFinal_ex returns 0 if the auth tag does not verify.
    const int verified = EVP_DecryptFinal_ex(
        ctx, reinterpret_cast<unsigned char*>(plain.data()) + len, &finalLen);
    EVP_CIPHER_CTX_free(ctx);
    OPENSSL_cleanse(key.data(), key.size());   // wipe derived key (§10)
    if (!ok || verified != 1) {
        OPENSSL_cleanse(plain.data(), plain.size());
        return false;
    }

    unflatten(plain, m_secrets);
    OPENSSL_cleanse(plain.data(), plain.size()); // wipe decrypted plaintext buffer
    return true;
}

bool CredentialVault::save(const QString& path, const QString& masterPassword) const {
    const QByteArray blob = encrypt(masterPassword);
    if (blob.isEmpty()) return false;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    return f.write(blob) == blob.size();
}

bool CredentialVault::load(const QString& path, const QString& masterPassword) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    return decrypt(f.readAll(), masterPassword);
}

} // namespace macxterm::core

// ── DPAPI-bound vault (Windows) ──
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dpapi.h>          // CryptProtectData / CryptUnprotectData

namespace macxterm::core {
namespace { constexpr char kDpapiMagic[8] = {'M','X','V','D','P','A','P','1'}; }

bool CredentialVault::dpapiAvailable() { return true; }

bool CredentialVault::saveDpapi(const QString& path) const {
    const QByteArray plain = flatten(m_secrets);
    DATA_BLOB in{ static_cast<DWORD>(plain.size()),
                  reinterpret_cast<BYTE*>(const_cast<char*>(plain.constData())) };
    DATA_BLOB out{};
    if (!CryptProtectData(&in, L"macXterm vault", nullptr, nullptr, nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN, &out))
        return false;
    QByteArray blob(kDpapiMagic, sizeof(kDpapiMagic));
    blob.append(reinterpret_cast<const char*>(out.pbData), int(out.cbData));
    LocalFree(out.pbData);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    return f.write(blob) == blob.size();
}

bool CredentialVault::loadDpapi(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QByteArray blob = f.readAll();
    if (blob.size() < int(sizeof(kDpapiMagic)) ||
        std::memcmp(blob.constData(), kDpapiMagic, sizeof(kDpapiMagic)) != 0)
        return false;
    QByteArray body = blob.mid(sizeof(kDpapiMagic));
    DATA_BLOB in{ static_cast<DWORD>(body.size()), reinterpret_cast<BYTE*>(body.data()) };
    DATA_BLOB out{};
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &out))
        return false;
    QByteArray plain(reinterpret_cast<const char*>(out.pbData), int(out.cbData));
    LocalFree(out.pbData);
    unflatten(plain, m_secrets);
    OPENSSL_cleanse(plain.data(), plain.size());
    return true;
}

} // namespace macxterm::core
#else
namespace macxterm::core {
bool CredentialVault::dpapiAvailable() { return false; }
bool CredentialVault::saveDpapi(const QString&) const { return false; }
bool CredentialVault::loadDpapi(const QString&) { return false; }
} // namespace macxterm::core
#endif
