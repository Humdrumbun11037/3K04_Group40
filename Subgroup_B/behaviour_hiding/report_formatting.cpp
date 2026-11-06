//report formatting

#include "behaviour_hiding/report_formatting.hpp"

#include <cstdio>
#include <vector>

using namespace hms::behaviour;

// ---- Turning seconds into a calendar date, by hand ----

static bool isLeapYear(long long year) {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

static long long daysInYear(long long year) {
    return isLeapYear(year) ? 366 : 365;
}

static int daysInMonth(long long year, int month) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && isLeapYear(year)) return 29;
    return days[month - 1];
}

// Count whole days from 1970-01-01, then step through years and months.
std::string ReportFormatting::formatTime(long long seconds) const {
    long long days = seconds / 86400;
    long long rest = seconds % 86400;
    if (rest < 0) {          // before 1970: borrow a day so rest is 0..86399
        rest += 86400;
        days -= 1;
    }

    long long year = 1970;
    while (days < 0) {
        year--;
        days += daysInYear(year);
    }
    while (days >= daysInYear(year)) {
        days -= daysInYear(year);
        year++;
    }
    int month = 1;
    while (days >= daysInMonth(year, month)) {
        days -= daysInMonth(year, month);
        month++;
    }

    char text[32];
    std::snprintf(text, sizeof text, "%04lld-%02d-%02lld %02lld:%02lld:%02lld",
                  year, month, days + 1, rest / 3600, rest % 3600 / 60, rest % 60);
    return text;
}

// ---- Layout ----

static std::string orAny(const std::string& value) {
    return value.empty() ? "any" : value;
}

static std::string entryLine(const ReportFormatting& f, const HistoryEntry& e) {
    return f.formatTime(e.time) + " " + e.who + " " + e.kind + " " + e.detail;
}

static std::string section(const ReportFormatting& f, const std::string& title,
                           const std::vector<HistoryEntry>& entries) {
    std::string text = title + " (" + std::to_string(entries.size()) + ")\n";
    if (entries.empty()) return text + "  (none)\n";
    for (const HistoryEntry& e : entries) text += "  " + entryLine(f, e) + "\n";
    return text;
}

std::string ReportFormatting::formatDaily(const DailyReport& report) const {
    std::string text = "DAILY REPORT\n";
    text += "Window: " + formatTime(report.from) + " to " + formatTime(report.to) + "\n";
    text += "\n";

    text += "ADCs needing attention (" + std::to_string(report.adcsNeedingAttention.size()) + ")\n";
    if (report.adcsNeedingAttention.empty()) text += "  (none)\n";
    for (const FlaggedAdc& adc : report.adcsNeedingAttention) {
        text += "  " + adcName(adc.adc) + ": " + std::to_string(adc.received) + " of "
              + std::to_string(adc.expected) + " expected heartbeats\n";
    }

    text += section(*this, "Inventory changes", report.inventoryChanges);
    text += section(*this, "Events", report.events);
    text += section(*this, "Alarms", report.alarms);
    text += section(*this, "ADC health", report.adcHealth);
    return text;
}

std::string ReportFormatting::formatSummary(const Summary& summary) const {
    const SummaryFilters& f = summary.filters;
    std::string text = "ACTIVITY SUMMARY\n";
    text += "Filters: adc=" + (f.adc == 0 ? std::string("any") : adcName(f.adc))
          + " medication=" + orAny(f.medication)
          + " actor=" + orAny(f.actor) + " from=" + formatTime(f.from)
          + " to=" + formatTime(f.to) + "\n";
    text += "Entries: " + std::to_string(summary.entries.size()) + "\n";
    for (const HistoryEntry& e : summary.entries) text += entryLine(*this, e) + "\n";
    return text;
}
