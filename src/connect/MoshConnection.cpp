#include "connect/MoshConnection.h"
#include <QProcess>

namespace macxterm::connect {

QStringList MoshConnection::buildArgs(const core::Session& session) {
    QStringList args;
    const QString key = session.param("keyfile");
    if (!key.isEmpty()) args << "--ssh" << QString("ssh -i %1").arg(key);
    const int port = session.port();
    if (port > 0 && port != 22) args << "--ssh" << QString("ssh -p %1").arg(port);
    QString target = session.host();
    if (!session.username().isEmpty()) target = session.username() + "@" + target;
    args << target;
    return args;
}

MoshConnection::MoshConnection(QObject* parent) : IConnection(parent) {}

bool MoshConnection::connectSession(const core::Session& session) {
    setState(State::Connecting);
    m_proc = new QProcess(this);
    connect(m_proc, &QProcess::readyReadStandardOutput, this, [this] {
        emit dataReceived(m_proc->readAllStandardOutput());
    });
    connect(m_proc, &QProcess::finished, this, [this](int) { setState(State::Closed); });
    m_proc->start("mosh", buildArgs(session));
    if (!m_proc->waitForStarted(3000)) {
        setState(State::Failed);
        emit errorOccurred(QStringLiteral("Failed to launch mosh (is it installed?)"));
        return false;
    }
    setState(State::Connected);
    return true;
}

qint64 MoshConnection::send(const QByteArray& data) {
    return m_proc ? m_proc->write(data) : -1;
}

void MoshConnection::disconnectSession() {
    if (m_proc) { m_proc->terminate(); m_proc->deleteLater(); m_proc = nullptr; }
    setState(State::Disconnected);
}

} // namespace macxterm::connect
