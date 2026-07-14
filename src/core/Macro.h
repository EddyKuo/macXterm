#pragma once
#include <QString>
#include <QByteArray>
#include <QList>
#include <functional>

namespace macxterm::core {

// Keystroke macro record/replay (research §1.2/§1.9). Recording appends raw
// byte chunks; replay feeds them to a sink. MobaXterm Home caps macros at 4 —
// macXterm imposes no such limit (PRD G3).
class Macro {
public:
    Macro() = default;
    explicit Macro(QString name) : m_name(std::move(name)) {}

    const QString& name() const { return m_name; }
    void setName(const QString& n) { m_name = n; }

    void beginRecording() { m_recording = true; m_events.clear(); }
    void endRecording() { m_recording = false; }
    bool isRecording() const { return m_recording; }

    // Record one keystroke chunk (ignored unless recording).
    void record(const QByteArray& chunk) { if (m_recording) m_events.push_back(chunk); }

    int eventCount() const { return m_events.size(); }

    // Replay all recorded events into `sink` in order.
    void replay(const std::function<void(const QByteArray&)>& sink) const {
        for (const QByteArray& e : m_events) sink(e);
    }

    // Serialize/deserialize for persistence (events joined by 0x00 boundaries
    // are unsafe; use a length-prefixed form).
    QByteArray serialize() const;
    static Macro deserialize(const QString& name, const QByteArray& blob);

private:
    QString m_name;
    bool m_recording = false;
    QList<QByteArray> m_events;
};

} // namespace macxterm::core
