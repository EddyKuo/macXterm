#include "core/SshConfigImporter.h"
#include <QFile>
#include <QStringList>
#include <QRegularExpression>

namespace macxterm::core {

namespace {
// A pending host block being assembled.
struct Pending {
    QString alias;
    QString hostName, user, identityFile, proxyJump;
    int port = 0;
    bool valid() const { return !alias.isEmpty() && !alias.contains('*') && !alias.contains('?'); }
};

void flush(Pending& p, SessionFolder& root) {
    if (!p.valid()) { p = Pending{}; return; }
    Session s(p.alias, SessionType::Ssh);
    s.setHost(p.hostName.isEmpty() ? p.alias : p.hostName);
    if (!p.user.isEmpty()) s.setUsername(p.user);
    if (p.port > 0) s.setPort(p.port);
    if (!p.identityFile.isEmpty()) s.setParam("keyfile", p.identityFile);
    if (!p.proxyJump.isEmpty()) s.setParam("jumphost", p.proxyJump);
    root.addSession(s);
    p = Pending{};
}
} // namespace

SessionFolder SshConfigImporter::parse(const QByteArray& configText) {
    SessionFolder root(QStringLiteral("Imported (ssh_config)"));
    Pending cur;
    const QStringList lines = QString::fromUtf8(configText).split('\n');
    for (QString raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        // Split "Keyword value..." — keyword is case-insensitive; '=' allowed.
        QString norm = line;
        norm.replace('=', ' ');
        const int sp = norm.indexOf(QRegularExpression("\\s"));
        if (sp < 0) continue;
        const QString key = norm.left(sp).toLower();
        const QString val = norm.mid(sp + 1).trimmed();
        if (val.isEmpty()) continue;

        if (key == "host") {
            flush(cur, root);        // close previous block
            cur.alias = val.section(' ', 0, 0);  // first pattern only
        } else if (key == "hostname") {
            cur.hostName = val;
        } else if (key == "user") {
            cur.user = val;
        } else if (key == "port") {
            cur.port = val.toInt();
        } else if (key == "identityfile") {
            cur.identityFile = val;
        } else if (key == "proxyjump") {
            cur.proxyJump = val;
        }
    }
    flush(cur, root);                // close final block
    return root;
}

SessionFolder SshConfigImporter::importFile(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return SessionFolder(QStringLiteral("Imported (ssh_config)"));
    return parse(f.readAll());
}

} // namespace macxterm::core
