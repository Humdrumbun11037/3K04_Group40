//daily schedule

/*
Hides: what daily functions are called

Receives: clock information
Sends: a command to events manager to see if anything needs to be added to long term storage,
       a prompt to report types to send the daily report if it is due
*/
#pragma once

#include <string>

#include "../external_interfaces.hpp"

class DailyTasks {
public:
    DailyTasks(const Clock& clock, AuditTrail& storage, DailyReportGenerator& dailyReport);

    // Call this from the main loop on every tick.
    // - The daily report is offered a chance to run on every tick; report
    //   types decides whether it is actually due (and retries on failure).
    // - Archiving runs on the first tick of each calendar day (including the
    //   very first tick after startup) and is skipped on every other tick.
    void tick();

    // The day ("YYYY-MM-DD") archiving last ran, or "" if it never has.
    const std::string& lastArchiveDay() const { return lastArchiveDay_; }

private:
    const Clock& clock_;
    AuditTrail& storage_;
    DailyReportGenerator& dailyReport_;
    std::string lastArchiveDay_;
};
