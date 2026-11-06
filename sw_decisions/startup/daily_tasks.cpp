//daily schedule

/*
Hides: what daily functions are called

Receives: clock information
Sends: a command to events manager to see if anything needs to be added to long term storage,
       a prompt to report types to send the daily report if it is due
*/

#include "daily_tasks.hpp"

DailyTasks::DailyTasks(const Clock& clock, AuditTrail& storage, DailyReportGenerator& dailyReport)
    : clock_(clock), storage_(storage), dailyReport_(dailyReport) {}

void DailyTasks::tick() {
    const std::string now = clock_.now();
    if (!timeutil::isValid(now)) return;   // bad clock reading: try again next tick

    // Daily report: report types knows when it is due (06:00) and whether
    // today's has been sent, so it is asked on every tick.
    dailyReport_.generateDaily(now);

    // Archiving: once per calendar day.
    const std::string today = now.substr(0, 10);   // "YYYY-MM-DD"
    if (today != lastArchiveDay_) {
        storage_.archiveOlderThan13Months(now);
        lastArchiveDay_ = today;
    }
}
