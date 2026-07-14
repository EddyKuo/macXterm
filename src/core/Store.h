#pragma once
#include "core/SessionTree.h"
#include <QString>

namespace macxterm::core {

// SQLite-backed persistence for the session tree and known hosts (Schema /
// DB_Design contracts). Secrets are NOT stored here — they live in the
// encrypted CredentialVault; `session.vault_ref` only references them.
//
// Usage: open(path) — pass ":memory:" for tests — then save/load the tree.
class Store {
public:
    ~Store();

    // Open (and create schema if new). Returns false on failure.
    bool open(const QString& path);
    void close();
    bool isOpen() const { return m_open; }

    // Persist the entire folder tree (replaces existing rows).
    bool saveTree(const SessionFolder& root);
    // Load the folder tree; returns an empty "Sessions" root if none stored.
    SessionFolder loadTree();

    // Known-hosts (host-key pinning). Returns false on constraint failure.
    bool upsertKnownHost(const QString& host, int port,
                         const QString& keyType, const QString& fingerprint);
    // Returns the stored fingerprint or empty string if unknown.
    QString knownHostFingerprint(const QString& host, int port, const QString& keyType);

    int schemaVersion();

private:
    bool applySchema();
    void saveFolder(const SessionFolder& folder, int parentId);

    bool m_open = false;
    QString m_connName;
};

} // namespace macxterm::core
