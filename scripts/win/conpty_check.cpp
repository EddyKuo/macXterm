// Standalone Win32 compile-check for the ConPTY code path used in
// src/platform/Pty.cpp (Windows branch). Cross-compiled from macOS with MinGW
// (x86_64-w64-mingw32-g++) to prove the Windows pseudo-console syscalls are
// valid Win32 — the real Pty.cpp guards this same logic behind #ifdef _WIN32
// and adds Qt glue. Build: scripts/win/build-check.sh
// ConPTY (CreatePseudoConsole/HPCON) is gated behind Windows 10 RS5 headers.
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00        // Windows 10
#endif
#ifndef NTDDI_VERSION
#define NTDDI_VERSION 0x0A000006   // NTDDI_WIN10_RS5 (10.0.17763)
#endif
#include <windows.h>   // ConPTY (CreatePseudoConsole/HPCON) is declared here once the version macros above are set
#include <vector>
#include <string>
#include <cstdint>

// Mirror of Pty.cpp's Windows start()/write()/resize()/read()/terminate().
struct WinPty {
    HPCON  hpc = nullptr;
    HANDLE inWrite = nullptr;
    HANDLE outRead = nullptr;
    HANDLE process = nullptr;
    long long pid = -1;

    bool start(const std::wstring& cmdLine, int cols, int rows) {
        HANDLE inRead = nullptr, inWrite2 = nullptr, outRead2 = nullptr, outWrite = nullptr;
        if (!CreatePipe(&inRead, &inWrite2, nullptr, 0)) return false;
        if (!CreatePipe(&outRead2, &outWrite, nullptr, 0)) {
            CloseHandle(inRead); CloseHandle(inWrite2); return false;
        }
        COORD size{ static_cast<SHORT>(cols), static_cast<SHORT>(rows) };
        if (FAILED(CreatePseudoConsole(size, inRead, outWrite, 0, &hpc))) {
            CloseHandle(inRead); CloseHandle(inWrite2);
            CloseHandle(outRead2); CloseHandle(outWrite);
            return false;
        }
        STARTUPINFOEXW si{};
        si.StartupInfo.cb = sizeof(si);
        SIZE_T bytes = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
        si.lpAttributeList =
            reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(HeapAlloc(GetProcessHeap(), 0, bytes));
        InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &bytes);
        UpdateProcThreadAttribute(si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                                  hpc, sizeof(hpc), nullptr, nullptr);

        std::vector<wchar_t> cmd(cmdLine.begin(), cmdLine.end());
        cmd.push_back(0);
        PROCESS_INFORMATION pi{};
        const BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE,
                                       EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr,
                                       &si.StartupInfo, &pi);
        DeleteProcThreadAttributeList(si.lpAttributeList);
        HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
        CloseHandle(inRead);
        CloseHandle(outWrite);
        if (!ok) { ClosePseudoConsole(hpc); CloseHandle(inWrite2); CloseHandle(outRead2); return false; }
        inWrite = inWrite2; outRead = outRead2;
        process = pi.hProcess; pid = pi.dwProcessId;
        CloseHandle(pi.hThread);
        return true;
    }

    long long write(const char* data, int len) {
        DWORD written = 0;
        if (!WriteFile(inWrite, data, len, &written, nullptr)) return -1;
        return written;
    }

    void resize(int cols, int rows) {
        if (hpc) ResizePseudoConsole(hpc, COORD{ static_cast<SHORT>(cols), static_cast<SHORT>(rows) });
    }

    long long readAvailable(char* buf, int cap) {
        DWORD avail = 0;
        if (!PeekNamedPipe(outRead, nullptr, 0, nullptr, &avail, nullptr)) return -1;
        if (avail == 0) return 0;
        DWORD toRead = avail < static_cast<DWORD>(cap) ? avail : static_cast<DWORD>(cap);
        DWORD read = 0;
        if (!ReadFile(outRead, buf, toRead, &read, nullptr)) return -1;
        return read;
    }

    void terminate() {
        if (hpc) { ClosePseudoConsole(hpc); hpc = nullptr; }
        if (inWrite) { CloseHandle(inWrite); inWrite = nullptr; }
        if (outRead) { CloseHandle(outRead); outRead = nullptr; }
        if (process) { CloseHandle(process); process = nullptr; }
        pid = -1;
    }
};

int main() {
    WinPty pty;
    if (pty.start(L"cmd.exe", 80, 24)) {
        pty.write("echo hi\r\n", 9);
        pty.resize(100, 30);
        char buf[256];
        pty.readAvailable(buf, sizeof(buf));
        pty.terminate();
    }
    return 0;
}
