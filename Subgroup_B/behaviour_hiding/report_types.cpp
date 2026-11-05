//report types

#include "behaviour_hiding/report_types.hpp"

using namespace hms::behaviour;

// ---- The rules. Everything that would change if the report contents change is here. ----

// Team assumption: all times are hospital local time, with no time-zone
// handling, so the day number and 06:00 come straight from the seconds.
static const long long SECONDS_PER_DAY = 24 * 60 * 60;
static const long long DAILY_REPORT_TIME = 6 * 60 * 60;   // 06:00, in seconds into the day
static const long long DAILY_WINDOW = 24 * 60 * 60;       // the report covers the last 24 hours

// One heartbeat is expected per period (the same 60 s as adc_heartbeats).
static const long long HEARTBEAT_PERIOD_SECONDS = 60;

enum class Section { InventoryChanges, Events, Alarms, AdcHealth };

// Which daily-report section a history entry belongs in, by the start of its
// kind. First matching row wins; anything not listed is an event.
// "AdcHealth" (from ADC heartbeats) and "Maintenance" (from maintenance) go in
// the ADC health section; "ReserveChange" (from hospital stock) is an inventory change.
static Section sectionOf(const std::string& kind) {
    struct Rule { const char* prefix; Section section; };
    static const Rule rules[] = {
        {"Inventory",   Section::InventoryChanges},
        {"Reserve",     Section::InventoryChanges},
        {"Alarm",       Section::Alarms},
        {"Adc",         Section::AdcHealth},
        {"Maintenance", Section::AdcHealth},
    };
    for (const Rule& rule : rules) {
        if (kind.rfind(rule.prefix, 0) == 0) return rule.section;   // kind starts with prefix
    }
    return Section::Events;
}

static bool matches(const HistoryEntry& entry, const SummaryFilters& filters) {
    return (filters.adc == 0 || entry.adc == filters.adc)
        && (filters.medication.empty() || entry.medication == filters.medication)
        && (filters.actor.empty() || entry.who == filters.actor);
}

static std::string verdictName(SummaryVerdict verdict) {
    switch (verdict) {
        case SummaryVerdict::Summarized:    return "Summarized";
        case SummaryVerdict::NotAuthorized: return "NotAuthorized";
        case SummaryVerdict::InvalidRange:  return "InvalidRange";
        case SummaryVerdict::ReadFailure:   return "ReadFailure";
        case SummaryVerdict::AuditFailure:  return "AuditFailure";
    }
    return "Unknown";
}

static std::string orAny(const std::string& value) {
    return value.empty() ? "any" : value;
}

static std::string orAny(AdcId adc) {
    return adc == 0 ? "any" : adcName(adc);
}

// ---- The operations ----

ReportTypes::ReportTypes(const Clock& clock, const HistoryReader& reader, History& history,
                         const HeartbeatCounter& heartbeats, const AdcRegistry& registry,
                         ReportSender& sender, const Permissions& permissions)
    : clock(clock), reader(reader), history(history), heartbeats(heartbeats),
      registry(registry), sender(sender), permissions(permissions) {}

// Rows are checked in table order, so the first matching row wins.
DailyVerdict ReportTypes::generateDaily(long long now) {
    long long today = now / SECONDS_PER_DAY;
    long long timeOfDay = now % SECONDS_PER_DAY;
    if (lastReportDay == today)                       return DailyVerdict::AlreadyDone;      // row 1
    if (timeOfDay < DAILY_REPORT_TIME)                return DailyVerdict::TooEarly;         // row 2
    DailyReport report;
    if (!buildDaily(now, report)) {                                                          // row 3
        logFailure(now, "daily report could not be generated");
        return DailyVerdict::GenerationFailed;
    }
    if (sender.send(report) != SendResult::Sent) {                                           // row 4
        logFailure(now, "daily report could not be sent");
        return DailyVerdict::SendFailed;
    }
    lastReportDay = today;                                                                   // row 5
    return DailyVerdict::Sent;
}

// Read-only: fills report from history and heartbeat counts; false if either fails.
// An empty window still gives a report, with empty sections.
bool ReportTypes::buildDaily(long long now, DailyReport& report) const {
    report.from = now - DAILY_WINDOW;
    report.to = now;

    HistoryReadResult read = reader.read(report.from, report.to);
    if (read.verdict != ReadVerdict::Ok) return false;
    for (const HistoryEntry& entry : read.entries) {
        switch (sectionOf(entry.kind)) {
            case Section::InventoryChanges: report.inventoryChanges.push_back(entry); break;
            case Section::Events:           report.events.push_back(entry);           break;
            case Section::Alarms:           report.alarms.push_back(entry);           break;
            case Section::AdcHealth:        report.adcHealth.push_back(entry);        break;
        }
    }

    int expected = static_cast<int>(DAILY_WINDOW / HEARTBEAT_PERIOD_SECONDS);
    for (const AdcId& adc : registry.activeAdcs()) {
        HeartbeatCountResult counted = heartbeats.count(adc, report.from, report.to);
        if (counted.verdict != CountVerdict::Ok) return false;
        if (counted.count < expected) report.adcsNeedingAttention.push_back({adc, counted.count, expected});
    }
    return true;
}

// If even this write fails there is nowhere left to report it; the next call retries anyway.
void ReportTypes::logFailure(long long now, const std::string& what) {
    history.record({"ReportFailed", "HMS", what, now, 0, ""});
}

// Rows are checked in table order, so the first matching row wins.
SummaryResult ReportTypes::summary(const User& user, const SummaryFilters& filters) {
    SummaryResult result{SummaryVerdict::Summarized, {filters, {}}};
    if (!permissions.mayPerform(user.role, Action::GenerateSummary)) {
        result.verdict = SummaryVerdict::NotAuthorized;                                      // row 1
    } else if (filters.from > filters.to) {
        result.verdict = SummaryVerdict::InvalidRange;                                       // row 2
    } else {
        // read() excludes its end, so to + 1 makes filters.to included
        HistoryReadResult read = reader.read(filters.from, filters.to + 1);
        if (read.verdict != ReadVerdict::Ok) {
            result.verdict = SummaryVerdict::ReadFailure;                                    // row 3
        } else {
            for (const HistoryEntry& entry : read.entries) {                                 // row 4
                if (matches(entry, filters)) result.summary.entries.push_back(entry);
            }
        }
    }
    // Written last, so the audit entry records the actual outcome.
    if (!audit(user, filters, result.verdict)) {
        return {SummaryVerdict::AuditFailure, {filters, {}}};
    }
    return result;
}

bool ReportTypes::audit(const User& user, const SummaryFilters& filters, SummaryVerdict verdict) {
    std::string detail = verdictName(verdict)
        + " adc=" + orAny(filters.adc)
        + " medication=" + orAny(filters.medication)
        + " actor=" + orAny(filters.actor)
        + " from=" + std::to_string(filters.from)
        + " to=" + std::to_string(filters.to);
    return history.record({"SummaryRequested", user.name, detail, clock.now(), 0, ""})
           == HistoryResult::Stored;
}
