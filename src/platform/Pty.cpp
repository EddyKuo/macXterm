#include "platform/Pty.h"
#include <QSocketNotifier>

#if defined(_WIN32)
// ── Windows: ConPTY-backed pseudo-console ──
// ConPTY (CreatePseudoConsole/HPCON) requires Windows 10 RS5-era headers.
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00        // Windows 10
#endif
#ifndef NTDDI_VERSION
#define NTDDI_VERSION 0x0A000006   // NTDDI_WIN10_RS5 (10.0.17763)
#endif
#include <windows.h>   // ConPTY is declared here once the version macros above are set
#include <QWinEventNotifier>
#include <vector>

namespace macxterm::platform {

Pty::Pty(QObject* parent) : QObject(parent) {}
Pty::~Pty() { terminate(); }

bool Pty::start(const QString& program, const QStringList& args, int cols, int rows,
                const QString& /*argv0*/) {
    if (isRunning()) return false;

    HANDLE inRead = nullptr, inWrite = nullptr, outRead = nullptr, outWrite = nullptr;
    if (!CreatePipe(&inRead, &inWrite, nullptr, 0)) return false;
    if (!CreatePipe(&outRead, &outWrite, nullptr, 0)) { CloseHandle(inRead); CloseHandle(inWrite); return false; }

    HPCON hpc = nullptr;
    COORD size{ static_cast<SHORT>(cols), static_cast<SHORT>(rows) };
    if (FAILED(CreatePseudoConsole(size, inRead, outWrite, 0, &hpc))) {
        CloseHandle(inRead); CloseHandle(inWrite); CloseHandle(outRead); CloseHandle(outWrite);
        return false;
    }

    // Build the STARTUPINFOEX with the pseudoconsole attribute.
    STARTUPINFOEXW si{};
    si.StartupInfo.cb = sizeof(si);
    SIZE_T bytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
    si.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(HeapAlloc(GetProcessHeap(), 0, bytes));
    InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &bytes);
    UpdateProcThreadAttribute(si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                              hpc, sizeof(hpc), nullptr, nullptr);

    QString cmdLine = program;
    for (const QString& a : args) cmdLine += " " + a;
    std::vector<wchar_t> cmd(cmdLine.size() + 1);
    cmdLine.toWCharArray(cmd.data());
    cmd[cmdLine.size()] = 0;

    PROCESS_INFORMATION pi{};
    const BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE,
                                   EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr,
                                   &si.StartupInfo, &pi);
    DeleteProcThreadAttributeList(si.lpAttributeList);
    HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
    CloseHandle(inRead);
    CloseHandle(outWrite);
    if (!ok) { ClosePseudoConsole(hpc); CloseHandle(inWrite); CloseHandle(outRead); return false; }

    m_hpc = hpc;
    m_inWrite = inWrite;
    m_outRead = outRead;
    m_process = pi.hProcess;
    m_pid = static_cast<long long>(pi.dwProcessId);
    CloseHandle(pi.hThread);
    // Poll the read pipe from a timer-driven pump (ConPTY has no waitable handle
    // for readability); a background reader thread is the production approach.
    return true;
}

qint64 Pty::write(const QByteArray& data) {
    if (!m_inWrite) return -1;
    DWORD written = 0;
    if (!WriteFile(m_inWrite, data.constData(), data.size(), &written, nullptr)) return -1;
    return written;
}

void Pty::resize(int cols, int rows) {
    if (m_hpc) ResizePseudoConsole(m_hpc, COORD{ static_cast<SHORT>(cols), static_cast<SHORT>(rows) });
}

qint64 Pty::readAvailable(QByteArray& out) {
    if (!m_outRead) return -1;
    DWORD avail = 0;
    if (!PeekNamedPipe(m_outRead, nullptr, 0, nullptr, &avail, nullptr)) return -1;
    if (avail == 0) return 0;
    QByteArray buf(int(avail), 0);
    DWORD read = 0;
    if (!ReadFile(m_outRead, buf.data(), avail, &read, nullptr)) return -1;
    out.append(buf.constData(), int(read));
    return read;
}

void Pty::onReadable() {
    QByteArray data;
    if (readAvailable(data) > 0) emit readyRead(data);
}

void Pty::terminate() {
    if (m_hpc) { ClosePseudoConsole(m_hpc); m_hpc = nullptr; }
    if (m_inWrite) { CloseHandle(m_inWrite); m_inWrite = nullptr; }
    if (m_outRead) { CloseHandle(m_outRead); m_outRead = nullptr; }
    if (m_process) { CloseHandle(m_process); m_process = nullptr; }
    m_pid = -1;
}

} // namespace macxterm::platform
#else

#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <signal.h>
#include <cerrno>
#if defined(__APPLE__)
#include <util.h>
#else
#include <pty.h>
#endif
#include <vector>

namespace macxterm::platform {

Pty::Pty(QObject* parent) : QObject(parent) {}

Pty::~Pty() { terminate(); }

bool Pty::start(const QString& program, const QStringList& args, int cols, int rows,
                const QString& argv0) {
    if (isRunning()) return false;

    struct winsize ws{};
    ws.ws_col = static_cast<unsigned short>(cols);
    ws.ws_row = static_cast<unsigned short>(rows);

    int master = -1;
    const pid_t pid = forkpty(&master, nullptr, nullptr, &ws);
    if (pid < 0) return false;

    if (pid == 0) {
        // Child: exec the shell/program. argv[0] may differ from the exec target
        // (e.g. "-zsh" to request a login shell that sources the full profile).
        const QByteArray prog = program.toLocal8Bit();
        std::vector<QByteArray> argvStore;
        argvStore.push_back(argv0.isEmpty() ? prog : argv0.toLocal8Bit());
        for (const QString& a : args) argvStore.push_back(a.toLocal8Bit());
        std::vector<char*> argv;
        for (auto& s : argvStore) argv.push_back(s.data());
        argv.push_back(nullptr);
        setenv("TERM", "xterm-256color", 1);
        // Ensure the shell/child tools run in a UTF-8 locale. When the app is
        // launched from Finder / as a .app bundle, the environment has no LANG,
        // so tools fall back to the C locale and mangle CJK/multibyte output.
        // Use overwrite=0 so an existing user locale (e.g. zh_TW.UTF-8) wins.
        setenv("LANG", "en_US.UTF-8", 0);
        setenv("LC_CTYPE", "en_US.UTF-8", 0);
        execvp(prog.constData(), argv.data());
        _exit(127); // exec failed
    }

    // Parent.
    m_master = master;
    m_pid = pid;
    fcntl(m_master, F_SETFL, O_NONBLOCK);
    m_notifier = new QSocketNotifier(m_master, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &Pty::onReadable);
    return true;
}

qint64 Pty::write(const QByteArray& data) {
    if (m_master < 0) return -1;
    return ::write(m_master, data.constData(), data.size());
}

void Pty::resize(int cols, int rows) {
    if (m_master < 0) return;
    struct winsize ws{};
    ws.ws_col = static_cast<unsigned short>(cols);
    ws.ws_row = static_cast<unsigned short>(rows);
    ioctl(m_master, TIOCSWINSZ, &ws);
}

qint64 Pty::readAvailable(QByteArray& out) {
    if (m_master < 0) return -1;
    char buf[4096];
    const ssize_t n = ::read(m_master, buf, sizeof(buf));
    if (n > 0) { out.append(buf, static_cast<int>(n)); return n; }
    if (n == 0) return 0;
    if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
    return -1;
}

void Pty::onReadable() {
    QByteArray data;
    const qint64 n = readAvailable(data);
    if (n > 0) {
        emit readyRead(data);
    } else if (n < 0) {
        int status = 0;
        if (m_pid > 0) waitpid(static_cast<pid_t>(m_pid), &status, WNOHANG);
        terminate();
        emit finished(WIFEXITED(status) ? WEXITSTATUS(status) : -1);
    }
}

void Pty::terminate() {
    if (m_notifier) { m_notifier->setEnabled(false); m_notifier->deleteLater(); m_notifier = nullptr; }
    if (m_master >= 0) { ::close(m_master); m_master = -1; }
    if (m_pid > 0) { ::kill(static_cast<pid_t>(m_pid), SIGHUP); m_pid = -1; }
}

} // namespace macxterm::platform
#endif
