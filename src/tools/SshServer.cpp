#include "tools/SshServer.h"

#if defined(MACXTERM_HAVE_LIBSSH) && !defined(_WIN32)
#define WITH_SERVER   // expose the sftp_server_* API in <libssh/sftp.h>
#include <libssh/libssh.h>
#include <libssh/server.h>
#include <libssh/sftp.h>
#include <QByteArray>
#include <QFileInfo>
#include <QDir>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <dirent.h>
#include <csignal>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <string>
#include <cstdint>
#include <atomic>
#include <chrono>

#if defined(__APPLE__)
#include <util.h>        // forkpty on macOS
#else
#include <pty.h>         // forkpty on Linux
#endif

namespace macxterm::tools {
namespace {

// Relay an interactive shell over the SSH channel using a PTY.
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

// Resolve an SFTP path against the server root, refusing escapes above it.
std::string resolveRooted(const QString& root, const char* path) {
    QString p = QString::fromUtf8(path);
    if (!p.startsWith('/')) p.prepend('/');
    const QString full = QDir::cleanPath(root + p);
    if (full != root && !full.startsWith(root + "/")) return {};   // path traversal guard
    return full.toStdString();
}

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
            // Directory handles were DIR*, file handles were fd-in-pointer. We
            // can't distinguish reliably, so try closedir then close.
            // Track via a small heuristic: fds are small positive ints.
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

// Handle one accepted SSH session start-to-finish. The key exchange runs in
// non-blocking mode with a stop-flag + deadline so a half-open client (or
// server shutdown) can't wedge the handler thread — otherwise stop() would hang
// joining it. After a successful handshake the session goes back to blocking for
// the auth/shell/sftp phase.
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
        struct timespec ts{0, 10'000'000}; nanosleep(&ts, nullptr);   // 10ms
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
    // Poll the listening fd ourselves with a timeout so the loop can honor
    // stop() promptly — ssh_bind_set_blocking(0) does not reliably make
    // ssh_bind_accept()'s underlying accept() non-blocking on all platforms, and
    // a blocked accept() would wedge this thread against join() in stop().
    const socket_t bindFd = ssh_bind_get_fd(sshbind);
    ssh_bind_set_blocking(sshbind, 0);
    const std::string user = m_user.toStdString(), pass = m_pass.toStdString();
    const QString root = m_root;

    while (!m_stop.load()) {
        struct pollfd pfd{bindFd, POLLIN, 0};
        if (::poll(&pfd, 1, 200) <= 0) continue;     // recheck m_stop every 200ms
        if (!(pfd.revents & POLLIN)) continue;
        ssh_session session = ssh_new();
        const int rc = ssh_bind_accept(sshbind, session);
        if (rc == SSH_OK) {
            // Track the handler thread (so stop() can join it — detaching left
            // handshakes running against freed state on shutdown → SIGABRT) and
            // its socket fd (so stop() can shutdown() it to unblock a blocked
            // read → otherwise the join would hang).
            const int fd = ssh_get_fd(session);
            auto t = std::make_shared<std::thread>(handleSession, session, user, pass, root, &m_stop);
            std::lock_guard<std::mutex> lock(m_sessionsMutex);
            m_sessions.push_back(std::move(t));
            m_sessionFds.push_back(fd);
        } else {
            ssh_free(session);
            struct timespec ts{0, 50'000'000}; nanosleep(&ts, nullptr);
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
    for (int fd : fds) if (fd >= 0) ::shutdown(fd, SHUT_RDWR);
    for (auto& t : sessions) if (t && t->joinable()) t->join();
    m_running = false;
}

} // namespace macxterm::tools

#else  // no libssh (or Windows)
namespace macxterm::tools {
SshServer::SshServer(QObject* parent) : QObject(parent) {}
SshServer::~SshServer() {}
bool SshServer::start(quint16, const QString&, const QString&, const QString&) { return false; }
void SshServer::stop() {}
void SshServer::acceptLoop() {}
} // namespace macxterm::tools
#endif
