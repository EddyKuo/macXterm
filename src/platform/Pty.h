#pragma once
#include <QObject>
#include <QByteArray>
#include <QStringList>

class QSocketNotifier;

namespace macxterm::platform {

// Cross-platform pseudo-terminal abstraction (Architecture §5, PAL).
// On Unix: forkpty(). On Windows: ConPTY (implemented in Pty_win.cpp — TODO).
// Emits readyRead() when child output is available; write() sends input.
class Pty : public QObject {
    Q_OBJECT
public:
    explicit Pty(QObject* parent = nullptr);
    ~Pty() override;

    // Launch `program` with `args` in a new PTY. Returns false on failure.
    bool start(const QString& program, const QStringList& args = {},
               int cols = 80, int rows = 24);

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
#else
    int m_master = -1;
    QSocketNotifier* m_notifier = nullptr;
#endif
};

} // namespace macxterm::platform
