#include "platform/WinIntegration.h"

#if defined(_WIN32)
#include <QCoreApplication>
#include <QDir>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace macxterm::platform {
namespace {

// Set the default (unnamed) value of HKCU\Software\Classes\<subkey> to `value`.
bool setClassesValue(const QString& subkey, const QString& value,
                     const wchar_t* namedValue = nullptr) {
    const QString full = QStringLiteral("Software\\Classes\\") + subkey;
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER,
                        reinterpret_cast<const wchar_t*>(full.utf16()),
                        0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS)
        return false;
    const std::wstring data(reinterpret_cast<const wchar_t*>(value.utf16()), value.size());
    const LONG rc = RegSetValueExW(key, namedValue, 0, REG_SZ,
                                   reinterpret_cast<const BYTE*>(data.c_str()),
                                   static_cast<DWORD>((data.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    return rc == ERROR_SUCCESS;
}

QString exePath() {
    return QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
}
QString openCommand() {
    return QLatin1Char('"') + exePath() + QStringLiteral("\" \"%1\"");
}
} // namespace

bool WinIntegration::available() { return true; }

bool WinIntegration::registerProtocolHandler() {
    bool ok = true;
    ok &= setClassesValue(QStringLiteral("macxterm"), QStringLiteral("URL:macXterm Protocol"));
    ok &= setClassesValue(QStringLiteral("macxterm"), QString(), L"URL Protocol");
    ok &= setClassesValue(QStringLiteral("macxterm\\DefaultIcon"),
                          QLatin1Char('"') + exePath() + QStringLiteral("\",0"));
    ok &= setClassesValue(QStringLiteral("macxterm\\shell\\open\\command"), openCommand());
    return ok;
}

bool WinIntegration::registerFileAssociation() {
    bool ok = true;
    ok &= setClassesValue(QStringLiteral(".mxtsession"), QStringLiteral("macXterm.Session"));
    ok &= setClassesValue(QStringLiteral("macXterm.Session"), QStringLiteral("macXterm Session"));
    ok &= setClassesValue(QStringLiteral("macXterm.Session\\DefaultIcon"),
                          QLatin1Char('"') + exePath() + QStringLiteral("\",0"));
    ok &= setClassesValue(QStringLiteral("macXterm.Session\\shell\\open\\command"), openCommand());
    return ok;
}

bool WinIntegration::registerAll() {
    // Evaluate both (avoid short-circuit) so a partial failure still registers what it can.
    const bool a = registerProtocolHandler();
    const bool b = registerFileAssociation();
    return a && b;
}

} // namespace macxterm::platform

#else
namespace macxterm::platform {
bool WinIntegration::available() { return false; }
bool WinIntegration::registerProtocolHandler() { return false; }
bool WinIntegration::registerFileAssociation() { return false; }
bool WinIntegration::registerAll() { return false; }
} // namespace macxterm::platform
#endif
