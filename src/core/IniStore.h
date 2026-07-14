#pragma once
#include "core/SessionTree.h"
#include <QString>
#include <QByteArray>

namespace macxterm::core {

// Persists the session tree to a portable INI-style text format (our own,
// with an importer for PuTTY/OpenSSH planned — research/MobaXterm.md §1.7).
//
// Format (one [Folder] header then session lines):
//   [Folder=Production]
//   web1=SSH;host=10.0.0.1;port=22;username=root
//   [Folder=Production/DB]
//   db1=SSH;host=10.0.0.2;username=admin
class IniStore {
public:
    // Serialize a folder subtree to bytes.
    static QByteArray serialize(const SessionFolder& root);
    // Parse bytes into a folder tree. Returns a root folder named "Sessions".
    static SessionFolder deserialize(const QByteArray& data);

    // File helpers (return false on I/O error).
    static bool save(const SessionFolder& root, const QString& path);
    static bool load(SessionFolder& out, const QString& path);
};

} // namespace macxterm::core
