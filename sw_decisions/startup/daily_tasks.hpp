//daily schedule

/*
Hides: what daily functions are called

Receives: clock information
Sends: a command to events manager to see if anything needs to be added to long term storage
*/
#pragma once

#include <string>

#include "../external_interfaces.hpp"

class DailyTasks {
public:
    DailyTasks(const Clock& clock, AuditTrail& storage);

    // Call this from the main loop as often as you like. The daily tasks run
    // on the first call of each calendar day (including the very first call
    // after startup) and are skipped on every other call that day.
    // Returns true if the daily tasks ran on this call.
    bool tick();

    // The day ("YYYY-MM-DD") the tasks last ran, or "" if they never have.
    const std::string& lastRunDay() const { return lastRunDay_; }

private:
    // The work done once per day.
    void runDailyTasks(const std::string& now);

    const Clock& clock_;
    AuditTrail& storage_;
    std::string lastRunDay_;
};
