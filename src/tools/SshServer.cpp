#include "tools/SshServer.h"

#if defined(MACXTERM_HAVE_LIBSSH)
#define WITH_SERVER   // expose the sftp_server_* API in <libssh/sftp.h>
#include <libssh/libssh.h>
#include <libssh/server.h>
#include <libssh/sftp.h>
#include "platform/Net.h"
#include <QByteArray>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <winsock2.h>
#include <windows.h>
#else
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <dirent.h>
#include <csignal>
#include <ctime>
#if defined(__APPLE__)
#include <util.h>        // forkpty on macOS
#else
#include <pty.h>         // forkpty on Linux
#endif
#endif

namespace macxterm::tools {
namespace {

// Resolve an SFTP path against the server root, refusing escapes above it.
// (Pure QDir/QString — shared by both platforms.)
std::string resolveRooted(const QString& root, const char* path) {
    QString p = QString::fromUtf8(path);
    if (!p.startsWith('/')) p.prepend('/');
    const QString full = QDir::cleanPath(root + p);
    if (full != root && !full.startsWith(root + "/")) return {};   // path traversal guard
    return full.toStdString();
}

// ─────────────────────────────── Interactive shell ───────────────────────────
#if defined(_WIN32)
// Windows: relay an interactive shell over the SSH channel using ConPTY.
void runShell(ssh_channel chan) {
    HANDLE inR = nullptr, inW = nullptr, outR = nullptr, outW = nullptr;
    if (!CreatePipe(&inR, &inW, nullptr, 0)) return;
    if (!CreatePipe(&outR, &outW, nullptr, 0)) { CloseHandle(inR); CloseHandle(inW); return; }

    HPCON hpc = nullptr;
    if (FAILED(CreatePseudoConsole(COORD{120, 30}, inR, outW, 0, &hpc))) {
        CloseHandle(inR); CloseHandle(inW); CloseHandle(outR); CloseHandle(outW); return;
    }
    STARTUPINFOEXW si{}; si.StartupInfo.cb = sizeof(si);
    SIZE_T bytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
    si.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(
        HeapAlloc(GetProcessHeap(), 0, bytes));
    InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &bytes);
    UpdateProcThreadAttribute(si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                              hpc, sizeof(hpc), nullptr, nullptr);

    const char* cs = std::getenv("ComSpec");
    std::wstring cmd = L"cmd.exe";
    if (cs && *cs) { int n = MultiByteToWideChar(CP_ACP, 0, cs, -1, nullptr, 0);
                     cmd.assign(n ? n - 1 : 0, L'\0');
                     MultiByteToWideChar(CP_ACP, 0, cs, -1, cmd.data(), n); }
    // Quote the program so CreateProcessW (lpApplicationName == nullptr) doesn't
    // split a spaced %ComSpec% path on spaces. Single token, no args appended.
    cmd = L'"' + cmd + L'"';
    std::vector<wchar_t> cmdbuf(cmd.begin(), cmd.end()); cmdbuf.push_back(0);

    PROCESS_INFORMATION pi{};
    const BOOL ok = CreateProcessW(nullptr, cmdbuf.data(), nullptr, nullptr, FALSE,
                                   EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr,
                                   &si.StartupInfo, &pi);
    DeleteProcThreadAttributeList(si.lpAttributeList);
    HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
    CloseHandle(inR); CloseHandle(outW);
    if (!ok) { ClosePseudoConsole(hpc); CloseHandle(inW); CloseHandle(outR); return; }

    char buf[16384];
    while (ssh_channel_is_open(chan) && !ssh_channel_is_eof(chan)) {
        DWORD avail = 0;
        if (PeekNamedPipe(outR, nullptr, 0, nullptr, &avail, nullptr) && avail > 0) {
            DWORD rd = 0;
            const DWORD want = avail < sizeof(buf) ? avail : (DWORD)sizeof(buf);
            if (ReadFile(outR, buf, want, &rd, nullptr) && rd > 0) ssh_channel_write(chan, buf, rd);
        }
        const int n = ssh_channel_read_nonblocking(chan, buf, sizeof(buf), 0);
        if (n > 0) { DWORD wr = 0; WriteFile(inW, buf, n, &wr, nullptr); }
        else if (n == SSH_ERROR) break;
        if (avail == 0 && WaitForSingleObject(pi.hProcess, 0) == WAIT_OBJECT_0) break;
        if (avail == 0 && n <= 0) Sleep(10);
    }
    ClosePseudoConsole(hpc);
    CloseHandle(inW); CloseHandle(outR);
    TerminateProcess(pi.hProcess, 0);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    ssh_channel_send_eof(chan);
}
#else
// Unix: relay an interactive shell over the SSH channel using a PTY (forkpty).
void runShell(ssh_channel chan) {
    int master = -1;
    const pid_t pid = forkpty(&master, nullptr, nullptr, nullptr);
    if (pid < 0) return;
    if (pid == 0) {
        const char* sh = getenv("SHELL");
        execl(sh && *sh ? sh : "/bin/sh", sh && *sh ? sh : "/bin/sh", "-i", nullptr);
        _exit(127);
    }
    fcntl(master, F_SETFL, O_NONBLOCK);
    char buf[16384];
    while (ssh_channel_is_open(chan) && !ssh_channel_is_eof(chan)) {
        struct pollfd pfd{master, POLLIN, 0};
        ::poll(&pfd, 1, 20);
        if (pfd.revents & POLLIN) {
            const ssize_t n = ::read(master, buf, sizeof(buf));
            if (n > 0) ssh_channel_write(chan, buf, n);
            else if (n == 0) break;
        }
        // channel -> pty
        const int n = ssh_channel_read_nonblocking(chan, buf, sizeof(buf), 0);
        if (n > 0) { if (::write(master, buf, n) < 0) break; }
        else if (n == SSH_ERROR) break;
        int status;
        if (waitpid(pid, &status, WNOHANG) == pid) break;
    }
    ::close(master);
    kill(pid, SIGHUP);
    waitpid(pid, nullptr, 0);
    ssh_channel_send_eof(chan);
}
#endif

// ──────────────────────────────── SFTP subsystem ─────────────────────────────
#if defined(_WIN32)
// Windows SFTP: a Qt-backed implementation (no POSIX fd/DIR). A handle is a small
// heap struct — a QFile for files, or a directory listing + cursor for dirs.
struct WinSftpHandle {
    bool isDir = false;
    QFile* file = nullptr;      // files
    QStringList entries;        // dirs
    int idx = 0;
};

void sftpReplyStat(sftp_client_message msg, const QString& path) {
    QFileInfo fi(path);
    if (!fi.exists()) { sftp_reply_status(msg, SSH_FX_NO_SUCH_FILE, "not found"); return; }
    sftp_attributes_struct attr{};
    attr.size = static_cast<uint64_t>(fi.size());
    attr.uid = 0; attr.gid = 0;
    attr.permissions = fi.isDir() ? (040000u | 0755u) : (0100000u | 0644u);
    attr.atime = static_cast<uint32_t>(fi.lastRead().toSecsSinceEpoch());
    attr.mtime = static_cast<uint32_t>(fi.lastModified().toSecsSinceEpoch());
    attr.flags = SSH_FILEXFER_ATTR_SIZE | SSH_FILEXFER_ATTR_UIDGID |
                 SSH_FILEXFER_ATTR_PERMISSIONS | SSH_FILEXFER_ATTR_ACMODTIME;
    sftp_reply_attr(msg, &attr);
}

void runSftp(ssh_session session, ssh_channel chan, const QString& root) {
    sftp_session sftp = sftp_server_new(session, chan);
    if (!sftp || sftp_server_init(sftp) != 0) { if (sftp) sftp_free(sftp); return; }

    sftp_client_message msg;
    while ((msg = sftp_get_client_message(sftp)) != nullptr) {
        const char* fname = sftp_client_message_get_filename(msg);
        const int type = sftp_client_message_get_type(msg);
        switch (type) {
        case SSH_FXP_REALPATH: {
            QString p = QString::fromUtf8(fname ? fname : ".");
            if (p == ".") p = "/";
            if (!p.startsWith('/')) p.prepend('/');
            sftp_attributes_struct attr{};
            attr.permissions = 040000u | 0755u;
            attr.flags = SSH_FILEXFER_ATTR_PERMISSIONS;
            const QByteArray cp = QDir::cleanPath(p).toUtf8();
            sftp_reply_name(msg, cp.constData(), &attr);
            break;
        }
        case SSH_FXP_STAT:
        case SSH_FXP_LSTAT: {
            const std::string full = resolveRooted(root, fname ? fname : "/");
            if (full.empty()) sftp_reply_status(msg, SSH_FX_PERMISSION_DENIED, "denied");
            else sftpReplyStat(msg, QString::fromStdString(full));
            break;
        }
        case SSH_FXP_OPENDIR: {
            const std::string full = resolveRooted(root, fname ? fname : "/");
            QDir dir(QString::fromStdString(full));
            if (full.empty() || !dir.exists()) { sftp_reply_status(msg, SSH_FX_NO_SUCH_FILE, "no dir"); break; }
            auto* h = new WinSftpHandle;
            h->isDir = true;
            h->entries = dir.entryList(QDir::AllEntries | QDir::Hidden | QDir::System);
            ssh_string sh = sftp_handle_alloc(sftp, h);
            sftp_reply_handle(msg, sh);
            ssh_string_free(sh);
            break;
        }
        case SSH_FXP_READDIR: {
            auto* h = static_cast<WinSftpHandle*>(sftp_handle(sftp, msg->handle));
            if (!h || !h->isDir) { sftp_reply_status(msg, SSH_FX_FAILURE, "bad handle"); break; }
            int count = 0;
            while (h->idx < h->entries.size() && count < 50) {
                const QByteArray name = h->entries[h->idx++].toUtf8();
                sftp_attributes_struct attr{};
                attr.name = strdup(name.constData());
                attr.longname = strdup(name.constData());
                attr.flags = SSH_FILEXFER_ATTR_PERMISSIONS;
                attr.permissions = 0100000u | 0644u;
                sftp_reply_names_add(msg, attr.name, attr.longname, &attr);
                free(attr.name); free(attr.longname);
                ++count;
            }
            if (count > 0) sftp_reply_names(msg);
            else sftp_reply_status(msg, SSH_FX_EOF, "eof");
            break;
        }
        case SSH_FXP_OPEN: {
            const std::string full = resolveRooted(root, fname ? fname : "");
            const int flags = sftp_client_message_get_flags(msg);
            QIODevice::OpenMode mode = QIODevice::NotOpen;
            if ((flags & SSH_FXF_WRITE) && (flags & SSH_FXF_READ)) mode = QIODevice::ReadWrite;
            else if (flags & SSH_FXF_WRITE) mode = QIODevice::WriteOnly;
            else mode = QIODevice::ReadOnly;
            if (flags & SSH_FXF_TRUNC) mode |= QIODevice::Truncate;
            if (flags & SSH_FXF_APPEND) mode |= QIODevice::Append;
            auto* f = new QFile(QString::fromStdString(full));
            if (full.empty() || !f->open(mode)) {
                delete f; sftp_reply_status(msg, SSH_FX_PERMISSION_DENIED, "open failed"); break;
            }
            auto* h = new WinSftpHandle; h->file = f;
            ssh_string sh = sftp_handle_alloc(sftp, h);
            sftp_reply_handle(msg, sh);
            ssh_string_free(sh);
            break;
        }
        case SSH_FXP_READ: {
            auto* h = static_cast<WinSftpHandle*>(sftp_handle(sftp, msg->handle));
            if (!h || !h->file) { sftp_reply_status(msg, SSH_FX_FAILURE, "bad handle"); break; }
            h->file->seek(static_cast<qint64>(msg->offset));
            const QByteArray data = h->file->read(msg->len ? msg->len : 16384);
            if (!data.isEmpty()) sftp_reply_data(msg, data.constData(), data.size());
            else sftp_reply_status(msg, SSH_FX_EOF, "eof");
            break;
        }
        case SSH_FXP_WRITE: {
            auto* h = static_cast<WinSftpHandle*>(sftp_handle(sftp, msg->handle));
            if (!h || !h->file) { sftp_reply_status(msg, SSH_FX_FAILURE, "bad handle"); break; }
            h->file->seek(static_cast<qint64>(msg->offset));
            const qint64 n = h->file->write(
                reinterpret_cast<const char*>(ssh_string_data(msg->data)),
                ssh_string_len(msg->data));
            sftp_reply_status(msg, n >= 0 ? SSH_FX_OK : SSH_FX_FAILURE, n >= 0 ? "" : "write failed");
            break;
        }
        case SSH_FXP_CLOSE: {
            auto* h = static_cast<WinSftpHandle*>(sftp_handle(sftp, msg->handle));
            if (h) {
                if (h->file) { h->file->close(); delete h->file; }
                sftp_handle_remove(sftp, h);
                delete h;
            }
            sftp_reply_status(msg, SSH_FX_OK, "");
            break;
        }
        case SSH_FXP_MKDIR: {
            const std::string full = resolveRooted(root, fname ? fname : "");
            sftp_reply_status(msg, (!full.empty() && QDir().mkpath(QString::fromStdString(full)))
                                       ? SSH_FX_OK : SSH_FX_FAILURE, "");
            break;
        }
        case SSH_FXP_RMDIR: {
            const std::string full = resolveRooted(root, fname ? fname : "");
            sftp_reply_status(msg, (!full.empty() && QDir().rmdir(QString::fromStdString(full)))
                                       ? SSH_FX_OK : SSH_FX_FAILURE, "");
            break;
        }
        case SSH_FXP_REMOVE: {
            const std::string full = resolveRooted(root, fname ? fname : "");
            sftp_reply_status(msg, (!full.empty() && QFile::remove(QString::fromStdString(full)))
                                       ? SSH_FX_OK : SSH_FX_FAILURE, "");
            break;
        }
        case SSH_FXP_SETSTAT:
        case SSH_FXP_FSETSTAT:
            sftp_reply_status(msg, SSH_FX_OK, "");
            break;
        default:
            sftp_reply_status(msg, SSH_FX_OP_UNSUPPORTED, "unsupported");
            break;
        }
        sftp_client_message_free(msg);
    }
    sftp_free(sftp);
}
#else
void sftpReplyStat(sftp_client_message msg, const std::string& path) {
    struct stat st{};
    if (::stat(path.c_str(), &st) != 0) {
        sftp_reply_status(msg, SSH_FX_NO_SUCH_FILE, "not found");
        return;
    }
    sftp_attributes_struct attr{};
    attr.size = st.st_size;
    attr.uid = st.st_uid; attr.gid = st.st_gid;
    attr.permissions = st.st_mode;
    attr.atime = st.st_atime; attr.mtime = st.st_mtime;
    attr.flags = SSH_FILEXFER_ATTR_SIZE | SSH_FILEXFER_ATTR_UIDGID |
                 SSH_FILEXFER_ATTR_PERMISSIONS | SSH_FILEXFER_ATTR_ACMODTIME;
    sftp_reply_attr(msg, &attr);
}

// A minimal SFTP subsystem server rooted at `root`. Handles the operations a
// file browser needs: realpath, stat/lstat, opendir/readdir, open/read/write/
// close, mkdir/rmdir/remove/setstat.
void runSftp(ssh_session session, ssh_channel chan, const QString& root) {
    sftp_session sftp = sftp_server_new(session, chan);
    if (!sftp || sftp_server_init(sftp) != 0) { if (sftp) sftp_free(sftp); return; }

    sftp_client_message msg;
    while ((msg = sftp_get_client_message(sftp)) != nullptr) {
        const char* fname = sftp_client_message_get_filename(msg);
        const int type = sftp_client_message_get_type(msg);
        switch (type) {
        case SSH_FXP_REALPATH: {
            QString p = QString::fromUtf8(fname ? fname : ".");
            if (p == ".") p = "/";
            if (!p.startsWith('/')) p.prepend('/');
            sftp_attributes_struct attr{};
            attr.permissions = S_IFDIR | 0755;
            attr.flags = SSH_FILEXFER_ATTR_PERMISSIONS;
            const QByteArray cp = QDir::cleanPath(p).toUtf8();
            sftp_reply_name(msg, cp.constData(), &attr);
            break;
        }
        case SSH_FXP_STAT:
        case SSH_FXP_LSTAT: {
            const std::string full = resolveRooted(root, fname ? fname : "/");
            if (full.empty()) sftp_reply_status(msg, SSH_FX_PERMISSION_DENIED, "denied");
            else sftpReplyStat(msg, full);
            break;
        }
        case SSH_FXP_OPENDIR: {
            const std::string full = resolveRooted(root, fname ? fname : "/");
            DIR* d = full.empty() ? nullptr : ::opendir(full.c_str());
            if (!d) { sftp_reply_status(msg, SSH_FX_NO_SUCH_FILE, "no dir"); break; }
            ssh_string h = sftp_handle_alloc(sftp, d);
            sftp_reply_handle(msg, h);
            ssh_string_free(h);
            break;
        }
        case SSH_FXP_READDIR: {
            DIR* d = static_cast<DIR*>(sftp_handle(sftp, msg->handle));
            if (!d) { sftp_reply_status(msg, SSH_FX_FAILURE, "bad handle"); break; }
            struct dirent* de = nullptr;
            int count = 0;
            while ((de = ::readdir(d)) != nullptr) {
                sftp_attributes_struct attr{};
                attr.name = strdup(de->d_name);
                attr.longname = strdup(de->d_name);
                attr.flags = SSH_FILEXFER_ATTR_PERMISSIONS;
                attr.permissions = (de->d_type == DT_DIR) ? (S_IFDIR | 0755) : (S_IFREG | 0644);
                sftp_reply_names_add(msg, attr.name, attr.longname, &attr);
                free(attr.name); free(attr.longname);
                if (++count >= 50) break;
            }
            if (count > 0) sftp_reply_names(msg);
            else sftp_reply_status(msg, SSH_FX_EOF, "eof");
            break;
        }
        case SSH_FXP_OPEN: {
            const std::string full = resolveRooted(root, fname ? fname : "");
            const int flags = sftp_client_message_get_flags(msg);
            int oflags = O_RDONLY;
            if ((flags & SSH_FXF_WRITE) && (flags & SSH_FXF_READ)) oflags = O_RDWR;
            else if (flags & SSH_FXF_WRITE) oflags = O_WRONLY;
            if (flags & SSH_FXF_CREAT) oflags |= O_CREAT;
            if (flags & SSH_FXF_TRUNC) oflags |= O_TRUNC;
            if (flags & SSH_FXF_APPEND) oflags |= O_APPEND;
            const int fd = full.empty() ? -1 : ::open(full.c_str(), oflags, 0644);
            if (fd < 0) { sftp_reply_status(msg, SSH_FX_PERMISSION_DENIED, "open failed"); break; }
            ssh_string h = sftp_handle_alloc(sftp, reinterpret_cast<void*>(static_cast<intptr_t>(fd)));
            sftp_reply_handle(msg, h);
            ssh_string_free(h);
            break;
        }
        case SSH_FXP_READ: {
            const int fd = static_cast<int>(reinterpret_cast<intptr_t>(sftp_handle(sftp, msg->handle)));
            std::string data(msg->len ? msg->len : 16384, '\0');
            const ssize_t n = ::pread(fd, data.data(), data.size(), msg->offset);
            if (n > 0) sftp_reply_data(msg, data.data(), static_cast<int>(n));
            else sftp_reply_status(msg, SSH_FX_EOF, "eof");
            break;
        }
        case SSH_FXP_WRITE: {
            const int fd = static_cast<int>(reinterpret_cast<intptr_t>(sftp_handle(sftp, msg->handle)));
            const ssize_t n = ::pwrite(fd, ssh_string_data(msg->data),
                                       ssh_string_len(msg->data), msg->offset);
            sftp_reply_status(msg, n >= 0 ? SSH_FX_OK : SSH_FX_FAILURE, n >= 0 ? "" : "write failed");
            break;
        }
        case SSH_FXP_CLOSE: {
            void* h = sftp_handle(sftp, msg->handle);
            const intptr_t v = reinterpret_cast<intptr_t>(h);
            if (v > 0 && v < 1 << 20) ::close(static_cast<int>(v));
            else if (h) ::closedir(static_cast<DIR*>(h));
            sftp_handle_remove(sftp, h);
            sftp_reply_status(msg, SSH_FX_OK, "");
            break;
        }
        case SSH_FXP_MKDIR: {
            const std::string full = resolveRooted(root, fname ? fname : "");
            sftp_reply_status(msg, (!full.empty() && ::mkdir(full.c_str(), 0755) == 0)
                                       ? SSH_FX_OK : SSH_FX_FAILURE, "");
            break;
        }
        case SSH_FXP_RMDIR: {
            const std::string full = resolveRooted(root, fname ? fname : "");
            sftp_reply_status(msg, (!full.empty() && ::rmdir(full.c_str()) == 0)
                                       ? SSH_FX_OK : SSH_FX_FAILURE, "");
            break;
        }
        case SSH_FXP_REMOVE: {
            const std::string full = resolveRooted(root, fname ? fname : "");
            sftp_reply_status(msg, (!full.empty() && ::unlink(full.c_str()) == 0)
                                       ? SSH_FX_OK : SSH_FX_FAILURE, "");
            break;
        }
        case SSH_FXP_SETSTAT:
        case SSH_FXP_FSETSTAT:
            sftp_reply_status(msg, SSH_FX_OK, "");
            break;
        default:
            sftp_reply_status(msg, SSH_FX_OP_UNSUPPORTED, "unsupported");
            break;
        }
        sftp_client_message_free(msg);
    }
    sftp_free(sftp);
}
#endif

// Handle one accepted SSH session start-to-finish. Key exchange runs non-blocking
// with a stop-flag + deadline so a half-open client can't wedge the handler
// thread. (Pure libssh + a portable sleep — shared by both platforms.)
void handleSession(ssh_session session, const std::string& user, const std::string& pass,
                   const QString& root, const std::atomic<bool>* stop) {
    ssh_set_blocking(session, 0);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    int rc;
    while ((rc = ssh_handle_key_exchange(session)) == SSH_AGAIN) {
        if ((stop && stop->load()) || !ssh_is_connected(session) ||
            std::chrono::steady_clock::now() > deadline) {
            ssh_disconnect(session); ssh_free(session); return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (rc != SSH_OK) { ssh_disconnect(session); ssh_free(session); return; }
    ssh_set_blocking(session, 1);

    // Password authentication.
    bool authed = false;
    ssh_message m;
    while (!authed && (m = ssh_message_get(session))) {
        if (ssh_message_type(m) == SSH_REQUEST_AUTH &&
            ssh_message_subtype(m) == SSH_AUTH_METHOD_PASSWORD) {
            const char* u = ssh_message_auth_user(m);
            const char* p = ssh_message_auth_password(m);
            if (u && p && user == u && pass == p) { authed = true; ssh_message_auth_reply_success(m, 0); }
            else { ssh_message_auth_set_methods(m, SSH_AUTH_METHOD_PASSWORD); ssh_message_reply_default(m); }
        } else {
            ssh_message_auth_set_methods(m, SSH_AUTH_METHOD_PASSWORD);
            ssh_message_reply_default(m);
        }
        ssh_message_free(m);
    }
    if (!authed) { ssh_disconnect(session); ssh_free(session); return; }

    // Accept a session channel.
    ssh_channel chan = nullptr;
    while (!chan && (m = ssh_message_get(session))) {
        if (ssh_message_type(m) == SSH_REQUEST_CHANNEL_OPEN &&
            ssh_message_subtype(m) == SSH_CHANNEL_SESSION)
            chan = ssh_message_channel_request_open_reply_accept(m);
        else
            ssh_message_reply_default(m);
        ssh_message_free(m);
    }
    if (!chan) { ssh_disconnect(session); ssh_free(session); return; }

    // Wait for a shell or an sftp subsystem request.
    bool shell = false, sftp = false;
    while (!shell && !sftp && (m = ssh_message_get(session))) {
        if (ssh_message_type(m) == SSH_REQUEST_CHANNEL) {
            const int st = ssh_message_subtype(m);
            if (st == SSH_CHANNEL_REQUEST_PTY) ssh_message_channel_request_reply_success(m);
            else if (st == SSH_CHANNEL_REQUEST_SHELL) { shell = true; ssh_message_channel_request_reply_success(m); }
            else if (st == SSH_CHANNEL_REQUEST_SUBSYSTEM) {
                const char* sub = ssh_message_channel_request_subsystem(m);
                if (sub && std::string(sub) == "sftp") { sftp = true; ssh_message_channel_request_reply_success(m); }
                else ssh_message_reply_default(m);
            } else ssh_message_reply_default(m);
        } else ssh_message_reply_default(m);
        ssh_message_free(m);
    }

    if (shell) runShell(chan);
    else if (sftp) runSftp(session, chan, root);

    ssh_channel_close(chan);
    ssh_channel_free(chan);
    ssh_disconnect(session);
    ssh_free(session);
}

} // namespace

SshServer::SshServer(QObject* parent) : QObject(parent) {}
SshServer::~SshServer() { stop(); }

bool SshServer::start(quint16 port, const QString& username, const QString& password,
                      const QString& rootDir) {
    stop();
    m_port = port; m_user = username; m_pass = password;
    m_root = QDir::cleanPath(rootDir.isEmpty() ? QDir::homePath() : rootDir);
    m_stop = false;

    ssh_init();
    m_running = true;
    m_thread = std::make_shared<std::thread>([this] { acceptLoop(); });
    return true;
}

void SshServer::acceptLoop() {
    ssh_bind sshbind = ssh_bind_new();
    if (!sshbind) { m_running = false; return; }

    // Generate an in-memory ED25519 host key.
    ssh_key hostkey = nullptr;
    if (ssh_pki_generate(SSH_KEYTYPE_ED25519, 0, &hostkey) != SSH_OK) {
        ssh_bind_free(sshbind); m_running = false; return;
    }
    const int port = m_port;
    ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_BINDADDR, "127.0.0.1");
    ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_BINDPORT, &port);
    ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_IMPORT_KEY, hostkey);

    if (ssh_bind_listen(sshbind) != SSH_OK) {
        ssh_bind_free(sshbind); m_running = false; return;
    }
    // Poll the listening fd ourselves with a timeout so the loop can honor stop()
    // promptly (a blocked accept() would wedge this thread against join()).
    const int bindFd = static_cast<int>(ssh_bind_get_fd(sshbind));
    ssh_bind_set_blocking(sshbind, 0);
    const std::string user = m_user.toStdString(), pass = m_pass.toStdString();
    const QString root = m_root;

    while (!m_stop.load()) {
        if (platform::net::pollReadable(bindFd, 200) <= 0) continue;   // recheck m_stop every 200ms
        ssh_session session = ssh_new();
        const int rc = ssh_bind_accept(sshbind, session);
        if (rc == SSH_OK) {
            // Track the handler thread (so stop() can join it) and its socket fd
            // (so stop() can shut it down to unblock a blocked read).
            const int fd = static_cast<int>(ssh_get_fd(session));
            auto t = std::make_shared<std::thread>(handleSession, session, user, pass, root, &m_stop);
            std::lock_guard<std::mutex> lock(m_sessionsMutex);
            m_sessions.push_back(std::move(t));
            m_sessionFds.push_back(fd);
        } else {
            ssh_free(session);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    ssh_bind_free(sshbind);
    m_running = false;
}

void SshServer::stop() {
    m_stop = true;
    if (m_thread && m_thread->joinable()) m_thread->join();   // accept loop: no more sessions added
    m_thread.reset();
    // Unblock any handler stuck in a blocking libssh read by shutting down its
    // socket, then join them so no thread outlives the server.
    std::vector<std::shared_ptr<std::thread>> sessions;
    std::vector<int> fds;
    {
        std::lock_guard<std::mutex> lock(m_sessionsMutex);
        sessions.swap(m_sessions);
        fds.swap(m_sessionFds);
    }
    for (int fd : fds) platform::net::shutdownBoth(fd);
    for (auto& t : sessions) if (t && t->joinable()) t->join();
    m_running = false;
}

} // namespace macxterm::tools

#else  // no libssh
namespace macxterm::tools {
SshServer::SshServer(QObject* parent) : QObject(parent) {}
SshServer::~SshServer() {}
bool SshServer::start(quint16, const QString&, const QString&, const QString&) { return false; }
void SshServer::stop() {}
void SshServer::acceptLoop() {}
} // namespace macxterm::tools
#endif
