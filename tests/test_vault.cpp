#include "core/CredentialVault.h"
#include <QtTest/QtTest>

using namespace macxterm::core;

class TestVault : public QObject {
    Q_OBJECT
private slots:
    void encryptDecryptRoundTrip() {
        CredentialVault v;
        v.setSecret("session:web1", "hunter2");
        v.setSecret("session:db", "s3cr3t");
        const QByteArray blob = v.encrypt("master-pw");
        QVERIFY(!blob.isEmpty());

        CredentialVault v2;
        QVERIFY(v2.decrypt(blob, "master-pw"));
        QCOMPARE(v2.count(), 2);
        QCOMPARE(v2.secret("session:web1"), QStringLiteral("hunter2"));
        QCOMPARE(v2.secret("session:db"), QStringLiteral("s3cr3t"));
    }

    void wrongPasswordFails() {
        CredentialVault v;
        v.setSecret("k", "value");
        const QByteArray blob = v.encrypt("correct");
        CredentialVault v2;
        QVERIFY(!v2.decrypt(blob, "wrong"));   // auth tag must reject
    }

    void tamperDetected() {
        CredentialVault v;
        v.setSecret("k", "value");
        QByteArray blob = v.encrypt("pw");
        blob[blob.size() - 1] = blob[blob.size() - 1] ^ 0xFF; // flip last cipher byte
        CredentialVault v2;
        QVERIFY(!v2.decrypt(blob, "pw"));      // GCM tag mismatch
    }

    void ciphertextIsNotPlaintext() {
        CredentialVault v;
        v.setSecret("k", "SUPERSECRET");
        const QByteArray blob = v.encrypt("pw");
        QVERIFY(!blob.contains("SUPERSECRET"));
    }

    void fileSaveLoad() {
        CredentialVault v;
        v.setSecret("id", "pw-value");
        const QString path = QDir::tempPath() + "/macxterm_test_vault.bin";
        QVERIFY(v.save(path, "master"));
        CredentialVault v2;
        QVERIFY(v2.load(path, "master"));
        QCOMPARE(v2.secret("id"), QStringLiteral("pw-value"));
        QFile::remove(path);
    }
};

QTEST_APPLESS_MAIN(TestVault)
#include "test_vault.moc"
