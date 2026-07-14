#pragma once
#include <QString>
#include <QDateTime>

namespace macxterm::tools {

// A standard 5-field cron expression: "min hour dom month dow".
// Each field supports "*", lists ("1,15"), ranges ("1-5"), steps ("*/10",
// "0-30/5") and numeric values. Day-of-week 0 and 7 both mean Sunday.
// Pure and unit-tested; CronServer uses it to decide when a job is due.
class CronExpr {
public:
    CronExpr() = default;
    explicit CronExpr(const QString& expr) { m_valid = parse(expr); }

    bool isValid() const { return m_valid; }

    // True if `when` (minute resolution) matches the expression.
    bool matches(const QDateTime& when) const;

private:
    bool parse(const QString& expr);
    // Each field is a 60/24/31/12/7-wide presence bitmask.
    bool m_valid = false;
    quint64 m_min = 0;      // bits 0..59
    quint32 m_hour = 0;     // bits 0..23
    quint32 m_dom = 0;      // bits 1..31
    quint16 m_month = 0;    // bits 1..12
    quint8  m_dow = 0;      // bits 0..6 (Sun=0)
};

} // namespace macxterm::tools
