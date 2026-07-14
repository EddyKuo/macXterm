#include "tools/CronServer.h"
#include <QTimer>
#include <QDateTime>
#include <QProcess>

namespace macxterm::tools {

CronServer::CronServer(QObject* parent) : QObject(parent) {
    m_timer = new QTimer(this);
    m_timer->setInterval(60'000);   // check once a minute
    connect(m_timer, &QTimer::timeout, this, &CronServer::tick);
}

bool CronServer::addJob(const QString& cronExpr, const QString& command) {
    CronExpr c(cronExpr);
    if (!c.isValid()) return false;
    m_jobs.push_back({cronExpr, command, c});
    return true;
}

void CronServer::clearJobs() { m_jobs.clear(); }

void CronServer::start() { if (!m_timer->isActive()) m_timer->start(); }
void CronServer::stop() { m_timer->stop(); }
bool CronServer::isRunning() const { return m_timer->isActive(); }

void CronServer::tick() {
    const QDateTime now = QDateTime::currentDateTime();
    for (const Job& j : m_jobs) {
        if (j.cron.matches(now)) {
            QProcess::startDetached(QStringLiteral("/bin/sh"),
                                    {QStringLiteral("-c"), j.command});
            emit jobRan(j.command);
        }
    }
}

} // namespace macxterm::tools
