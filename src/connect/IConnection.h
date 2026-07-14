#pragma once
#include "core/Session.h"
#include <QObject>
#include <QByteArray>

namespace macxterm::connect {

// Unified connection abstraction (Architecture §6.1). Every protocol
// (SSH/Telnet/Serial/Mosh/RDP/VNC/local shell) implements this so the UI and
// terminal are protocol-agnostic. Data flows: dataReceived() -> VtEngine::input;
// TerminalWidget input -> send().
class IConnection : public QObject {
    Q_OBJECT
public:
    enum class State { Disconnected, Connecting, Connected, Failed, Closed };

    // What this connection can offer, so the UI can show/hide SFTP panel, X11, etc.
    struct Capabilities {
        bool sftp = false;
        bool x11 = false;
        bool tunnel = false;
        bool gui = false;   // RDP/VNC render their own surface, not a VT stream
    };

    explicit IConnection(QObject* parent = nullptr) : QObject(parent) {}
    ~IConnection() override = default;

    virtual bool connectSession(const core::Session& session) = 0;
    virtual void disconnectSession() = 0;
    virtual qint64 send(const QByteArray& data) = 0;
    virtual void resize(int cols, int rows) { Q_UNUSED(cols); Q_UNUSED(rows); }
    virtual Capabilities capabilities() const = 0;

    State state() const { return m_state; }

signals:
    void dataReceived(const QByteArray& data);
    void stateChanged(macxterm::connect::IConnection::State state);
    void errorOccurred(const QString& message);

protected:
    void setState(State s) {
        if (s != m_state) { m_state = s; emit stateChanged(s); }
    }

private:
    State m_state = State::Disconnected;
};

} // namespace macxterm::connect
