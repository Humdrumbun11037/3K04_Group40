//report formatting

/*
Hides: how reports look (FR-REP-01, FR-REP-02, NFR-USE-01): section order and
       titles, line layout, and how a time in seconds is shown as a local date
       and time (YYYY-MM-DD HH:MM:SS), worked out by hand with no time-zone library

Interface
takes: a daily report or a summary (data from report types), or a time
sends: the same thing as plain text
uses: report types (its data types only)
*/
#pragma once

#include <string>

#include "behaviour_hiding/report_types.hpp"

class ReportFormatting {
public:
    // { Pre: seconds is hospital local time since 1970-01-01 00:00 (may be negative) }
    //   formatTime(seconds)
    // { Post: returns "YYYY-MM-DD HH:MM:SS" }
    std::string formatTime(long long seconds) const;

    // { Pre: none }
    //   formatDaily(report)
    // { Post: returns a "DAILY REPORT" header, a window line, a blank line, then the
    //         sections ADCs needing attention, Inventory changes, Events, Alarms and
    //         ADC health, each titled with its count; an empty section shows "(none)" }
    std::string formatDaily(const DailyReport& report) const;

    // { Pre: none }
    //   formatSummary(summary)
    // { Post: returns an "ACTIVITY SUMMARY" header, a filter line, an entry count,
    //         then one line per entry: "date time who kind detail" }
    std::string formatSummary(const Summary& summary) const;
};
