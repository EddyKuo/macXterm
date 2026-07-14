#include "core/Store.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <functional>

namespace macxterm::core {

namespace {
int s_counter = 0;   // unique connection names for concurrent Store instances
}

Store::~Store() { close(); }

bool Store::open(const QString& path) {
    close();
    m_connName = QStringLiteral("macxterm_store_%1").arg(++s_counter);
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", m_connName);
    db.setDatabaseName(path);
    if (!db.open()) return false;
    QSqlQuery(db).exec("PRAGMA foreign_keys = ON");
    m_open = true;
    return applySchema();
}

void Store::close() {
    if (m_open) {
        { QSqlDatabase::database(m_connName).close(); }
        QSqlDatabase::removeDatabase(m_connName);
        m_open = false;
    }
}

bool Store::applySchema() {
    QSqlDatabase db = QSqlDatabase::database(m_connName);
    QSqlQuery q(db);
    const char* ddl[] = {
        "CREATE TABLE IF NOT EXISTS schema_version(version INTEGER NOT NULL)",
        "CREATE TABLE IF NOT EXISTS session_folder("
        " id INTEGER PRIMARY KEY, parent_id INTEGER REFERENCES session_folder(id) ON DELETE CASCADE,"
        " name TEXT NOT NULL)",
        "CREATE TABLE IF NOT EXISTS session("
        " id INTEGER PRIMARY KEY, folder_id INTEGER NOT NULL REFERENCES session_folder(id) ON DELETE CASCADE,"
        " name TEXT NOT NULL, type TEXT NOT NULL, host TEXT, port INTEGER, username TEXT, vault_ref TEXT)",
        "CREATE TABLE IF NOT EXISTS session_param("
        " session_id INTEGER NOT NULL REFERENCES session(id) ON DELETE CASCADE,"
        " key TEXT NOT NULL, value TEXT, PRIMARY KEY(session_id,key))",
        "CREATE TABLE IF NOT EXISTS known_host("
        " id INTEGER PRIMARY KEY, host TEXT NOT NULL, port INTEGER NOT NULL DEFAULT 22,"
        " key_type TEXT NOT NULL, fingerprint TEXT NOT NULL, UNIQUE(host,port,key_type))",
    };
    for (const char* stmt : ddl) {
        if (!q.exec(stmt)) return false;
    }
    q.exec("SELECT COUNT(*) FROM schema_version");
    if (q.next() && q.value(0).toInt() == 0) {
        q.exec("INSERT INTO schema_version(version) VALUES(1)");
    }
    return true;
}

int Store::schemaVersion() {
    QSqlQuery q(QSqlDatabase::database(m_connName));
    if (q.exec("SELECT MAX(version) FROM schema_version") && q.next())
        return q.value(0).toInt();
    return 0;
}

void Store::saveFolder(const SessionFolder& folder, int parentId) {
    QSqlDatabase db = QSqlDatabase::database(m_connName);
    QSqlQuery q(db);
    q.prepare("INSERT INTO session_folder(parent_id,name) VALUES(?,?)");
    if (parentId < 0) q.bindValue(0, QVariant()); else q.bindValue(0, parentId);
    q.bindValue(1, folder.name());
    q.exec();
    const int folderId = q.lastInsertId().toInt();

    for (const Session& s : folder.sessions()) {
        QSqlQuery sq(db);
        sq.prepare("INSERT INTO session(folder_id,name,type,host,port,username,vault_ref)"
                   " VALUES(?,?,?,?,?,?,?)");
        sq.bindValue(0, folderId);
        sq.bindValue(1, s.name());
        sq.bindValue(2, sessionTypeToString(s.type()));
        sq.bindValue(3, s.host());
        sq.bindValue(4, s.port());
        sq.bindValue(5, s.username());
        sq.bindValue(6, s.param("vault_ref"));
        sq.exec();
        const int sid = sq.lastInsertId().toInt();
        const QVariantMap& p = s.params();
        for (auto it = p.constBegin(); it != p.constEnd(); ++it) {
            QSqlQuery pq(db);
            pq.prepare("INSERT OR REPLACE INTO session_param(session_id,key,value) VALUES(?,?,?)");
            pq.bindValue(0, sid);
            pq.bindValue(1, it.key());
            pq.bindValue(2, it.value().toString());
            pq.exec();
        }
    }
    for (const auto& sub : folder.folders()) saveFolder(*sub, folderId);
}

bool Store::saveTree(const SessionFolder& root) {
    if (!m_open) return false;
    QSqlDatabase db = QSqlDatabase::database(m_connName);
    db.transaction();
    QSqlQuery(db).exec("DELETE FROM session_folder");  // cascades to sessions/params
    saveFolder(root, -1);
    return db.commit();
}

SessionFolder Store::loadTree() {
    SessionFolder root(QStringLiteral("Sessions"));
    if (!m_open) return root;
    QSqlDatabase db = QSqlDatabase::database(m_connName);

    // Find the top-level folder row (parent_id IS NULL).
    QSqlQuery q(db);
    q.exec("SELECT id,name FROM session_folder WHERE parent_id IS NULL LIMIT 1");
    if (!q.next()) return root;
    const int rootId = q.value(0).toInt();
    root.setName(q.value(1).toString());

    std::function<void(int, SessionFolder&)> fill = [&](int folderId, SessionFolder& f) {
        QSqlQuery sq(db);
        sq.prepare("SELECT id,name,type,host,port,username,vault_ref FROM session WHERE folder_id=?");
        sq.bindValue(0, folderId);
        sq.exec();
        while (sq.next()) {
            Session s(sq.value(1).toString(), sessionTypeFromString(sq.value(2).toString()));
            if (!sq.value(3).isNull()) s.setHost(sq.value(3).toString());
            if (!sq.value(4).isNull()) s.setPort(sq.value(4).toInt());
            if (!sq.value(5).isNull()) s.setUsername(sq.value(5).toString());
            if (!sq.value(6).toString().isEmpty()) s.setParam("vault_ref", sq.value(6).toString());
            const int sid = sq.value(0).toInt();
            QSqlQuery pq(db);
            pq.prepare("SELECT key,value FROM session_param WHERE session_id=?");
            pq.bindValue(0, sid);
            pq.exec();
            while (pq.next()) s.setParam(pq.value(0).toString(), pq.value(1).toString());
            f.addSession(s);
        }
        QSqlQuery fq(db);
        fq.prepare("SELECT id,name FROM session_folder WHERE parent_id=?");
        fq.bindValue(0, folderId);
        fq.exec();
        while (fq.next()) {
            SessionFolder* child = f.addFolder(fq.value(1).toString());
            fill(fq.value(0).toInt(), *child);
        }
    };
    fill(rootId, root);
    return root;
}

bool Store::upsertKnownHost(const QString& host, int port,
                            const QString& keyType, const QString& fingerprint) {
    if (!m_open) return false;
    QSqlQuery q(QSqlDatabase::database(m_connName));
    q.prepare("INSERT INTO known_host(host,port,key_type,fingerprint) VALUES(?,?,?,?)"
              " ON CONFLICT(host,port,key_type) DO UPDATE SET fingerprint=excluded.fingerprint");
    q.bindValue(0, host);
    q.bindValue(1, port);
    q.bindValue(2, keyType);
    q.bindValue(3, fingerprint);
    return q.exec();
}

QString Store::knownHostFingerprint(const QString& host, int port, const QString& keyType) {
    QSqlQuery q(QSqlDatabase::database(m_connName));
    q.prepare("SELECT fingerprint FROM known_host WHERE host=? AND port=? AND key_type=?");
    q.bindValue(0, host);
    q.bindValue(1, port);
    q.bindValue(2, keyType);
    if (q.exec() && q.next()) return q.value(0).toString();
    return {};
}

} // namespace macxterm::core
