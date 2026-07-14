#pragma once
#include <QString>
#include <QVariantMap>

namespace macxterm::core {

// Session type parity with MobaXterm's session manager (research/MobaXterm.md §1.1).
enum class SessionType {
    Ssh, Telnet, Rsh, Rlogin, Serial, Mosh, Sftp, Ftp, S3,
    Shell, Rdp, Vnc, Xdmcp, Browser, Unknown
};

QString sessionTypeToString(SessionType t);
SessionType sessionTypeFromString(const QString& s);

// A single saved connection profile (bookmark). Protocol-specific options
// live in `params` keyed by well-known names ("host", "port", "username", ...).
class Session {
public:
    Session() = default;
    Session(QString name, SessionType type) : m_name(std::move(name)), m_type(type) {}

    const QString& name() const { return m_name; }
    void setName(const QString& n) { m_name = n; }

    SessionType type() const { return m_type; }
    void setType(SessionType t) { m_type = t; }

    // Convenience typed accessors over the generic param bag.
    QString host() const { return param("host"); }
    void setHost(const QString& h) { setParam("host", h); }

    int port() const;
    void setPort(int p) { setParam("port", QString::number(p)); }

    QString username() const { return param("username"); }
    void setUsername(const QString& u) { setParam("username", u); }

    QString param(const QString& key, const QString& def = QString()) const {
        return m_params.value(key, def).toString();
    }
    void setParam(const QString& key, const QString& value) { m_params.insert(key, value); }
    const QVariantMap& params() const { return m_params; }
    void setParams(const QVariantMap& p) { m_params = p; }

    bool operator==(const Session& o) const {
        return m_name == o.m_name && m_type == o.m_type && m_params == o.m_params;
    }

private:
    QString m_name;
    SessionType m_type = SessionType::Unknown;
    QVariantMap m_params;
};

} // namespace macxterm::core
