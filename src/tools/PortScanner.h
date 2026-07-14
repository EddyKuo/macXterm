#pragma once
#include <QObject>
#include <QString>
#include <QList>

namespace macxterm::tools {

// TCP connect() port scanner (research §1.6 network tools). Synchronous helper
// scanPort() plus an async range scan that emits results as they arrive.
class PortScanner : public QObject {
    Q_OBJECT
public:
    explicit PortScanner(QObject* parent = nullptr) : QObject(parent) {}

    // Blocking single-port check: true if a TCP connection succeeds within
    // `timeoutMs`. Safe to call from tests against a loopback listener.
    static bool scanPort(const QString& host, quint16 port, int timeoutMs = 500);

    // Scan an inclusive port range; emits portOpen for each open port and
    // finished() when done. Runs on the caller's event loop.
    void scanRange(const QString& host, quint16 first, quint16 last, int timeoutMs = 300);

signals:
    void portOpen(quint16 port);
    void finished(int openCount);

private:
    QString m_host;
};

} // namespace macxterm::tools
