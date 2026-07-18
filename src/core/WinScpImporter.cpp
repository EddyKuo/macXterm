#include "core/WinScpImporter.h"
#include <QMap>
#include <QFile>

namespace macxterm::core {

namespace {
QString decodeName(const QString& raw) {
    return QString::fromUtf8(QByteArray::fromPercentEncoding(raw.toUtf8()));
}

// Build a Session from a WinSCP session's key/value settings. FSProtocol picks
// SFTP vs FTP (5 = FTP; everything else = SFTP/SCP).
Session sessionFromKv(const QString& name, const QMap<QString, QString>& kv) {
    const int fsproto = kv.value(QStringLiteral("FSProtocol")).toInt();
    const SessionType type = (fsproto == 5) ? SessionType::Ftp : SessionType::Sftp;
    Session s(name, type);
    const QString host = kv.value(QStringLiteral("HostName"));
    if (!host.isEmpty()) s.setHost(host);
    if (const int p = kv.value(QStringLiteral("PortNumber")).toInt(); p > 0) s.setPort(p);
    if (!kv.value(QStringLiteral("UserName")).isEmpty())
        s.setUsername(kv.value(QStringLiteral("UserName")));
    if (!kv.value(QStringLiteral("PublicKeyFile")).isEmpty())
        s.setParam(QStringLiteral("keyfile"), kv.value(QStringLiteral("PublicKeyFile")));
    return s;
}
} // namespace

SessionFolder WinScpImporter::parseIni(const QByteArray& iniText) {
    SessionFolder root(QStringLiteral("Imported (WinSCP)"));
    QString section;
    QMap<QString, QString> kv;

    auto flush = [&] {
        // Section form: "Sessions\<encoded name>". Skip the template session.
        if (section.startsWith(QLatin1String("Sessions\\"))) {
            const QString enc = section.mid(QStringLiteral("Sessions\\").size());
            if (enc != QLatin1String("Default%20Settings") && !enc.isEmpty()) {
                Session s = sessionFromKv(decodeName(enc), kv);
                if (!s.param(QStringLiteral("host")).isEmpty()) root.addSession(s);
            }
        }
        kv.clear();
    };

    const QStringList lines = QString::fromUtf8(iniText).split('\n');
    for (const QString& raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith(';')) continue;
        if (line.startsWith('[') && line.endsWith(']')) {
            flush();
            section = line.mid(1, line.size() - 2);
            continue;
        }
        const int eq = line.indexOf('=');
        if (eq <= 0) continue;
        kv.insert(line.left(eq).trimmed(), line.mid(eq + 1).trimmed());
    }
    flush();
    return root;
}

SessionFolder WinScpImporter::importFile(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return SessionFolder(QStringLiteral("Imported (WinSCP)"));
    return parseIni(f.readAll());
}

} // namespace macxterm::core

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace macxterm::core {
namespace {
QString wregStr(HKEY key, const wchar_t* name) {
    DWORD type = 0, bytes = 0;
    if (RegQueryValueExW(key, name, nullptr, &type, nullptr, &bytes) != ERROR_SUCCESS ||
        type != REG_SZ || bytes == 0)
        return QString();
    std::wstring buf(bytes / sizeof(wchar_t), L'\0');
    if (RegQueryValueExW(key, name, nullptr, nullptr,
                         reinterpret_cast<LPBYTE>(buf.data()), &bytes) != ERROR_SUCCESS)
        return QString();
    return QString::fromWCharArray(buf.c_str());
}
int wregDword(HKEY key, const wchar_t* name) {
    DWORD val = 0, bytes = sizeof(val), type = 0;
    if (RegQueryValueExW(key, name, nullptr, &type, reinterpret_cast<LPBYTE>(&val), &bytes)
            != ERROR_SUCCESS || type != REG_DWORD)
        return 0;
    return static_cast<int>(val);
}
} // namespace

SessionFolder WinScpImporter::importFromRegistry() {
    SessionFolder root(QStringLiteral("Imported (WinSCP)"));
    HKEY hSessions = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Martin Prikryl\\WinSCP 2\\Sessions",
                      0, KEY_READ, &hSessions) != ERROR_SUCCESS)
        return root;

    for (DWORD i = 0;; ++i) {
        wchar_t nameBuf[256];
        DWORD nameLen = 256;
        const LONG rc = RegEnumKeyExW(hSessions, i, nameBuf, &nameLen,
                                      nullptr, nullptr, nullptr, nullptr);
        if (rc == ERROR_NO_MORE_ITEMS) break;
        if (rc != ERROR_SUCCESS) continue;
        const QString enc = QString::fromWCharArray(nameBuf);
        if (enc == QLatin1String("Default%20Settings")) continue;

        HKEY hOne = nullptr;
        if (RegOpenKeyExW(hSessions, nameBuf, 0, KEY_READ, &hOne) != ERROR_SUCCESS) continue;
        QMap<QString, QString> kv;
        const QString host = wregStr(hOne, L"HostName");
        if (!host.isEmpty()) kv.insert(QStringLiteral("HostName"), host);
        if (const QString u = wregStr(hOne, L"UserName"); !u.isEmpty())
            kv.insert(QStringLiteral("UserName"), u);
        if (const QString k = wregStr(hOne, L"PublicKeyFile"); !k.isEmpty())
            kv.insert(QStringLiteral("PublicKeyFile"), k);
        if (const int p = wregDword(hOne, L"PortNumber"); p > 0)
            kv.insert(QStringLiteral("PortNumber"), QString::number(p));
        kv.insert(QStringLiteral("FSProtocol"), QString::number(wregDword(hOne, L"FSProtocol")));
        RegCloseKey(hOne);

        if (host.isEmpty()) continue;
        Session s = sessionFromKv(QString::fromUtf8(QByteArray::fromPercentEncoding(enc.toUtf8())), kv);
        root.addSession(s);
    }
    RegCloseKey(hSessions);
    return root;
}

} // namespace macxterm::core
#else
namespace macxterm::core {
SessionFolder WinScpImporter::importFromRegistry() {
    return SessionFolder(QStringLiteral("Imported (WinSCP)"));
}
} // namespace macxterm::core
#endif
