#pragma once
#include <QObject>
#include <QByteArray>
#include <QStringList>

#if defined(_WIN32)
#include <thread>
#include <atomic>
#endif

class QSocketNotifier;

namespace macxterm::platform {

// Cross-platform pseudo-terminal abstraction (Architecture §5, PAL).
// On Unix: forkpty() + QSocketNotifier. On Windows: ConPTY
// (CreatePseudoConsole) with a background reader thread — both in Pty.cpp.
// Emits readyRead() when child output is available; write() sends input.
class Pty : public QObject {
    Q_OBJECT
public:
    explicit Pty(QObject* parent = nullptr);
    ~Pty() override;

    // Launch `program` with `args` in a new PTY. Returns false on failure.
    // `argv0`, when non-empty, overrides argv[0] (the exec target stays
    // `program`) — pass "-zsh" etc. to start `program` as a login shell.
    // `workDir`, when non-empty, is the child's initial working directory; if it
    // cannot be entered the child keeps the inherited cwd rather than failing.
    bool start(const QString& program, const QStringList& args = {},
               int cols = 80, int rows = 24, const QString& argv0 = QString(),
               const QString& workDir = QString());

    bool isRunning() const { return m_pid > 0; }
    qint64 write(const QByteArray& data);
    void resize(int cols, int rows);
    void terminate();

    // Synchronous read (used by tests / non-Qt-event contexts). Returns bytes
    // read, 0 on EOF/no-data-yet, -1 on error.
    qint64 readAvailable(QByteArray& out);

signals:
    void readyRead(const QByteArray& data);
    void finished(int exitCode);

private slots:
    void onReadable();

private:
    long long m_pid = -1;
#if defined(_WIN32)
    void* m_hpc = nullptr;       // HPCON pseudoconsole
    void* m_inWrite = nullptr;   // HANDLE — write side of child stdin
    void* m_outRead = nullptr;   // HANDLE — read side of child stdout
    void* m_process = nullptr;   // HANDLE — child process
    // ConPTY has no waitable "readable" handle, so a dedicated background thread
    // does a blocking ReadFile loop on m_outRead and marshals each chunk back to
    // the GUI thread via a queued readyRead(). It also waits on the child and
    // emits finished() once the pipe closes. m_readerStop suppresses that
    // finished() when we are the ones tearing the PTY down (terminate()).
    std::thread m_reader;
    std::atomic<bool> m_readerStop{false};
#else
    int m_master = -1;
    QSocketNotifier* m_notifier = nullptr;
#endif
};

} // namespace macxterm::platform
