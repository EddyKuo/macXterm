#include "connect/MoshConnection.h"
#include <QProcess>

namespace macxterm::connect {

QStringList MoshConnection::buildArgs(const core::Session& session) {
    QStringList args;

    // The SSH bootstrap command: mosh accepts a single --ssh value, so fold the
    // key file and SSH port into one string. Emitting --ssh twice (as before)
    // silently dropped the first — mosh keeps only the last occurrence.
    QString ssh = QStringLiteral("ssh");
    const QString key = session.param("keyfile");
    if (!key.isEmpty()) ssh += QStringLiteral(" -i %1").arg(key);
    const int port = session.port();
    if (port > 0 && port != 22) ssh += QStringLiteral(" -p %1").arg(port);
    if (ssh != QLatin1String("ssh")) args << "--ssh" << ssh;

    // UDP port (range) for the mosh-server side (MobaXterm "Network port"); lets
    // roaming work through a firewall that only opens specific UDP ports.
    const QString moshPort = session.param("moshport");
    if (!moshPort.isEmpty()) args << "-p" << moshPort;

    // Predictive local echo mode (adaptive|always|never|experimental).
    const QString predict = session.param("predict");
    if (!predict.isEmpty()) args << QStringLiteral("--predict=%1").arg(predict);

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
