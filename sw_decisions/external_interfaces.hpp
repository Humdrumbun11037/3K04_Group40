//external interfaces

/*
Not a module of its own. This header lists the services that the
sw_decisions modules need from the hw_hiding and behav_hiding modules,
written as abstract interfaces. The hw_hiding / behav_hiding modules
implement these classes; sw_decisions only ever depends on the interfaces,
so those modules can change their secrets without touching sw_decisions.

    Interface          Implemented by (module_diagram.txt)
    ---------          -----------------------------------
    Clock              hw_hiding/clock
    CommsLink          hw_hiding/comms_link
    DashboardOutput    hw_hiding/dashboard_manager
    AuditTrail         hw_hiding/long_term_storage (audit_trail + records-searching)
    AlarmSeverityRules behav_hiding/alarms/alarm_severity
    AlarmResolver      behav_hiding/alarms/alarm_resolution
    ReportTypes        behav_hiding/reports (report_types + report_formatting)
    DailyReportGenerator behav_hiding/reports/report_types (generateDaily)
    InventoryStore     behav_hiding/inventory_manager/inventory_record
    HeartbeatMonitor   behav_hiding/messaging_period/adc_heartbeats
*/
#pragma once

#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

#include "events_logs/event_formatting.h"

// ---------------------------------------------------------------- clock
class Clock {
public:
    virtual ~Clock() = default;
    virtual std::string now() const = 0;   // "YYYY-MM-DD HH:MM:SS", e.g. "2026-10-30 14:05:09"
};

// Plain time arithmetic on the timestamp format above (no hardware involved).
namespace timeutil {

// Seconds since 1970-01-01 00:00:00 for a "YYYY-MM-DD HH:MM:SS" string.
// Returns false if the string is malformed.
inline bool toSeconds(const std::string& ts, long long& out) {
    int y, mo, d, h, mi, s;
    char tail;
    if (ts.size() != 19 ||
        std::sscanf(ts.c_str(), "%4d-%2d-%2d %2d:%2d:%2d%c", &y, &mo, &d, &h, &mi, &s, &tail) != 6)
        return false;
    if (mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59 || s > 59 ||
        h < 0 || mi < 0 || s < 0)
        return false;
    // days_from_civil (Howard Hinnant)
    y -= mo <= 2;
    const long long era = (y >= 0 ? y : y - 399) / 400;
    const long long yoe = y - era * 400;
    const long long doy = (153LL * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    const long long days = era * 146097 + doe - 719468;
    out = days * 86400LL + h * 3600LL + mi * 60LL + s;
    return true;
}

inline bool isValid(const std::string& ts) {
    long long ignored;
    return toSeconds(ts, ignored);
}

// later - earlier, in seconds. Returns 0 if either timestamp is malformed.
inline long long secondsBetween(const std::string& earlier, const std::string& later) {
    long long a, b;
    if (!toSeconds(earlier, a) || !toSeconds(later, b)) return 0;
    return b - a;
}

} // namespace timeutil

// ---------------------------------------------------------------- comms link
class CommsLink {
public:
    virtual ~CommsLink() = default;
    // Next encrypted packet received from any ADC, or nullopt if none waiting.
    virtual std::optional<std::vector<uint8_t>> receive() = 0;
};

// ---------------------------------------------------------------- dashboard
class DashboardOutput {
public:
    virtual ~DashboardOutput() = default;
    // Puts a message on one user's dashboard.
    virtual void deliver(const std::string& userId, const std::string& message) = 0;
};

// ---------------------------------------------------------------- long term storage
class AuditTrail {
public:
    virtual ~AuditTrail() = default;
    virtual void append(const Event& event) = 0;
    // All stored events with from <= timestamp <= to, oldest first.
    virtual std::vector<Event> search(const std::string& from, const std::string& to) const = 0;
    // Moves events more than 13 months older than `now` out of the live
    // audit trail into the archive. Called once per day by startup/daily_tasks.
    virtual void archiveOlderThan13Months(const std::string& now) = 0;
};

// ---------------------------------------------------------------- alarms
enum class AlarmSeverity { Low, High, Critical };

class AlarmSeverityRules {
public:
    virtual ~AlarmSeverityRules() = default;
    // Decides the severity of an alarm from the code the ADC sent.
    virtual AlarmSeverity classify(const std::string& alarmCode) const = 0;
};

class AlarmResolver {
public:
    virtual ~AlarmResolver() = default;
    // Starts tracking a newly raised alarm.
    virtual void track(int alarmId) = 0;
    // The "unresolved flag": false until a user has resolved the alarm.
    virtual bool isResolved(int alarmId) const = 0;
};

// ---------------------------------------------------------------- reports
class ReportTypes {
public:
    virtual ~ReportTypes() = default;
    virtual bool isKnownType(const std::string& reportType) const = 0;
    // Whether an event belongs in a report of the given type.
    virtual bool includes(const std::string& reportType, const Event& event) const = 0;
    // Lays out the finished report.
    virtual std::string format(const std::string& reportType,
                               const std::string& from,
                               const std::string& to,
                               const std::vector<Event>& events) const = 0;
};

// The daily report. When it is due, what it contains, and retrying after a
// failure are all decided by behav_hiding/reports (report_types); sw_decisions
// only gives it the chance to run. Called on every tick by startup/daily_tasks.
class DailyReportGenerator {
public:
    virtual ~DailyReportGenerator() = default;
    // Sends today's report if it is due and not yet sent; otherwise does nothing.
    virtual void generateDaily(const std::string& now) = 0;
};

// ---------------------------------------------------------------- inventory
class InventoryStore {
public:
    virtual ~InventoryStore() = default;
    virtual bool hasMedication(int adcId, const std::string& medication) const = 0;
    virtual int count(int adcId, const std::string& medication) const = 0;
    virtual void setCount(int adcId, const std::string& medication, int count) = 0;
};

// ---------------------------------------------------------------- heartbeats
class HeartbeatMonitor {
public:
    virtual ~HeartbeatMonitor() = default;
    virtual void heartbeat(int adcId, const std::string& timestamp) = 0;
};
