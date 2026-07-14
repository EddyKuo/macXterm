#pragma once
#include "tools/CronExpr.h"
#include <QObject>
#include <QString>
#include <QList>

class QTimer;

namespace macxterm::tools {

// A small in-process cron daemon (MobaXterm's embedded CRON server). Holds a
// list of (schedule, command) jobs and, once started, checks every minute and
// runs any due job via the system shell. No 360-second cap. Scheduling logic is
// in CronExpr (unit-tested); this class is the runtime around it.
class CronServer : public QObject {
    Q_OBJECT
public:
    explicit CronServer(QObject* parent = nullptr);

    struct Job { QString expr; QString command; CronExpr cron; };

    // Add a job. Returns false if the cron expression is invalid.
    bool addJob(const QString& cronExpr, const QString& command);
    void clearJobs();
    int jobCount() const { return m_jobs.size(); }

    void start();
    void stop();
    bool isRunning() const;

signals:
    void jobRan(const QString& command);

private slots:
    void tick();

private:
    QList<Job> m_jobs;
    QTimer* m_timer = nullptr;
};

} // namespace macxterm::tools
