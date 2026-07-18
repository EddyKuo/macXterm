#include "core/PuttyImporter.h"
#include <QMap>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace macxterm::core {

namespace {
// PuTTY encodes session names (registry key / file name) with %XX for characters
// like space (%20). Decode to a display name.
QString decodeName(const QString& raw) {
    return QString::fromUtf8(QByteArray::fromPercentEncoding(raw.toUtf8()));
}
} // namespace

Session PuttyImporter::parseSession(const QString& name, const QByteArray& settings) {
    // Collect "Key\value" / "Key=value" lines. Unix PuTTY files use a backslash
    // separator and a trailing backslash; registry-derived text uses '='.
    QMap<QString, QString> kv;
    const QStringList lines = QString::fromUtf8(settings).split('\n');
    for (const QString& raw : lines) {
        QString line = raw.trimmed();
        if (line.isEmpty()) continue;
        int sep = line.indexOf('\\');
        const int eq = line.indexOf('=');
        if (sep < 0 || (eq >= 0 && eq < sep)) sep = eq;
        if (sep <= 0) continue;
        const QString key = line.left(sep).trimmed();
        QString val = line.mid(sep + 1);
        if (val.endsWith('\\')) val.chop(1);
        kv.insert(key, val.trimmed());   // last occurrence wins
    }

    const QString proto = kv.value(QStringLiteral("Protocol")).toLower();
    SessionType type = SessionType::Ssh;
    if (proto == QLatin1String("telnet") || proto == QLatin1String("raw"))
        type = SessionType::Telnet;
    else if (proto == QLatin1String("rlogin"))
        type = SessionType::Rlogin;
    else if (proto == QLatin1String("serial"))
        type = SessionType::Serial;

    Session s(name, type);
    const QString host = kv.value(QStringLiteral("HostName"));
    if (!host.isEmpty()) s.setHost(host);
    if (const int p = kv.value(QStringLiteral("PortNumber")).toInt(); p > 0) s.setPort(p);
    if (!kv.value(QStringLiteral("UserName")).isEmpty())
        s.setUsername(kv.value(QStringLiteral("UserName")));
    if (!kv.value(QStringLiteral("PublicKeyFile")).isEmpty())
        s.setParam(QStringLiteral("keyfile"), kv.value(QStringLiteral("PublicKeyFile")));
    if (kv.value(QStringLiteral("Compression")) == QLatin1String("1"))
        s.setParam(QStringLiteral("compression"), QStringLiteral("1"));

    // Proxy → jump/gateway host (best-effort: [user@]host[:port]).
    const QString proxyHost = kv.value(QStringLiteral("ProxyHost"));
    if (!proxyHost.isEmpty()) {
        QString gw = proxyHost;
        const QString pp = kv.value(QStringLiteral("ProxyPort"));
        if (!pp.isEmpty() && pp != QLatin1String("0")) gw += QLatin1Char(':') + pp;
        const QString pu = kv.value(QStringLiteral("ProxyUsername"));
        if (!pu.isEmpty()) gw = pu + QLatin1Char('@') + gw;
        s.setParam(QStringLiteral("gateway"), gw);
    }

    if (type == SessionType::Serial) {
        if (!kv.value(QStringLiteral("SerialLine")).isEmpty())
            s.setParam(QStringLiteral("port"), kv.value(QStringLiteral("SerialLine")));
        if (const int b = kv.value(QStringLiteral("SerialSpeed")).toInt(); b > 0)
            s.setParam(QStringLiteral("baud"), QString::number(b));
    }
    return s;
}

SessionFolder PuttyImporter::importFromDir(const QString& dir) {
    SessionFolder root(QStringLiteral("Imported (PuTTY)"));
    QDir d(dir);
    if (!d.exists()) return root;
    const QFileInfoList files = d.entryInfoList(QDir::Files);
    for (const QFileInfo& fi : files) {
        QFile f(fi.absoluteFilePath());
        if (!f.open(QIODevice::ReadOnly)) continue;
        Session s = parseSession(decodeName(fi.fileName()), f.readAll());
        if (!s.param(QStringLiteral("host")).isEmpty()) root.addSession(s);
    }
    return root;
}

} // namespace macxterm::core

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace macxterm::core {
namespace {
// Read a REG_SZ value as a QString ("" if absent / wrong type).
QString regStr(HKEY key, const wchar_t* name) {
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
// Read a REG_DWORD value as int (0 if absent).
int regDword(HKEY key, const wchar_t* name) {
    DWORD val = 0, bytes = sizeof(val), type = 0;
    if (RegQueryValueExW(key, name, nullptr, &type, reinterpret_cast<LPBYTE>(&val), &bytes)
            != ERROR_SUCCESS || type != REG_DWORD)
        return 0;
    return static_cast<int>(val);
}
} // namespace

SessionFolder PuttyImporter::importFromRegistry() {
    SessionFolder root(QStringLiteral("Imported (PuTTY)"));
    HKEY hSessions = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\SimonTatham\\PuTTY\\Sessions",
                      0, KEY_READ, &hSessions) != ERROR_SUCCESS)
        return root;

    for (DWORD i = 0;; ++i) {
        wchar_t nameBuf[256];
        DWORD nameLen = 256;
        const LONG rc = RegEnumKeyExW(hSessions, i, nameBuf, &nameLen,
                                      nullptr, nullptr, nullptr, nullptr);
        if (rc == ERROR_NO_MORE_ITEMS) break;
        if (rc != ERROR_SUCCESS) continue;

        HKEY hOne = nullptr;
        if (RegOpenKeyExW(hSessions, nameBuf, 0, KEY_READ, &hOne) != ERROR_SUCCESS) continue;

        // Re-serialise the values into the neutral "Key=value" form parseSession
        // understands, so the mapping logic lives in one place.
        QByteArray body;
        auto add = [&body](const char* k, const QString& v) {
            if (!v.isEmpty()) body += QByteArray(k) + "=" + v.toUtf8() + "\n";
        };
        add("HostName", regStr(hOne, L"HostName"));
        add("Protocol", regStr(hOne, L"Protocol"));
        add("UserName", regStr(hOne, L"UserName"));
        add("PublicKeyFile", regStr(hOne, L"PublicKeyFile"));
        add("ProxyHost", regStr(hOne, L"ProxyHost"));
        add("ProxyUsername", regStr(hOne, L"ProxyUsername"));
        add("SerialLine", regStr(hOne, L"SerialLine"));
        if (const int port = regDword(hOne, L"PortNumber"); port > 0)
            add("PortNumber", QString::number(port));
        if (const int cmp = regDword(hOne, L"Compression"); cmp)
            add("Compression", QStringLiteral("1"));
        if (const int pp = regDword(hOne, L"ProxyPort"); pp > 0)
            add("ProxyPort", QString::number(pp));
        if (const int sp = regDword(hOne, L"SerialSpeed"); sp > 0)
            add("SerialSpeed", QString::number(sp));
        RegCloseKey(hOne);

        const QString name = decodeName(QString::fromWCharArray(nameBuf));
        Session s = parseSession(name, body);
        if (!s.param(QStringLiteral("host")).isEmpty() || s.type() == SessionType::Serial)
            root.addSession(s);
    }
    RegCloseKey(hSessions);
    return root;
}

} // namespace macxterm::core
#else
namespace macxterm::core {
// Non-Windows: PuTTY sessions live in files (~/.putty/sessions) — use
// importFromDir(). There is no registry to read.
SessionFolder PuttyImporter::importFromRegistry() {
    return SessionFolder(QStringLiteral("Imported (PuTTY)"));
}
} // namespace macxterm::core
#endif
