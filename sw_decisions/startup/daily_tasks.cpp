//daily schedule

/*
Hides: what daily functions are called

Receives: clock information
Sends: a command to events manager to see if anything needs to be added to long term storage
*/

#include "daily_tasks.hpp"

DailyTasks::DailyTasks(const Clock& clock, AuditTrail& storage)
    : clock_(clock), storage_(storage) {}

bool DailyTasks::tick() {
    const std::string now = clock_.now();
    if (!timeutil::isValid(now)) return false;   // bad clock reading: try again next tick

    const std::string today = now.substr(0, 10);   // "YYYY-MM-DD"
    if (today == lastRunDay_) return false;        // already done today

    runDailyTasks(now);
    lastRunDay_ = today;
    return true;
}

//This function must be called once per day
void DailyTasks::runDailyTasks(const std::string& now) {
    storage_.archiveOlderThan13Months(now);
}
