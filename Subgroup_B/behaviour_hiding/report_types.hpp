//report types

/*
Hides: what each report contains and which filters apply (FR-REP-01, FR-REP-02,
       NFR-TRC-01): when the daily report is due (06:00, once per local day),
       its 24-hour window and sections, which history entries go in which
       section, when an ADC needs attention, and who may request a summary.
       No layout: report formatting turns this data into text.

Interface
takes: the time (daily report), a user and filters (activity summary)
sends: the daily report to the report sender; summaries to the caller;
       failures and audit entries to history
uses: clock, history (read and write), heartbeat counter, ADC registry, report sender

Invariants:
- at most one daily report is sent per local day
- a day counts as done only after its report was sent successfully
- every summary request, accepted or rejected, writes exactly one audit entry
*/
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "behaviour_hiding/interfaces/adc_id.hpp"
#include "behaviour_hiding/interfaces/adc_registry.hpp"
#include "behaviour_hiding/interfaces/clock.hpp"
#include "behaviour_hiding/interfaces/heartbeat_counter.hpp"
#include "behaviour_hiding/interfaces/history.hpp"
#include "behaviour_hiding/interfaces/history_reader.hpp"
#include "behaviour_hiding/interfaces/report_sender.hpp"
#include "behaviour_hiding/interfaces/user.hpp"

// An ADC that sent fewer heartbeats than expected in the report window.
struct FlaggedAdc {
    hms::behaviour::AdcId adc;
    int received;
    int expected;
};

struct DailyReport {
    long long from;   // window is [from, to)
    long long to;
    std::vector<FlaggedAdc> adcsNeedingAttention;
    std::vector<hms::behaviour::HistoryEntry> inventoryChanges;
    std::vector<hms::behaviour::HistoryEntry> events;
    std::vector<hms::behaviour::HistoryEntry> alarms;
    std::vector<hms::behaviour::HistoryEntry> adcHealth;
};

enum class DailyVerdict { Sent, AlreadyDone, TooEarly, GenerationFailed, SendFailed };

// Empty adc, medication or actor means "any". The window is [from, to], both included.
struct SummaryFilters {
    hms::behaviour::AdcId adc;
    std::string medication;
    std::string actor;
    long long from;
    long long to;
};

struct Summary {
    SummaryFilters filters;
    std::vector<hms::behaviour::HistoryEntry> entries;   // oldest first
};

enum class SummaryVerdict { Summarized, NotAuthorized, InvalidRange, ReadFailure, AuditFailure };

struct SummaryResult {
    SummaryVerdict verdict;
    Summary summary;   // entries empty unless verdict == Summarized
};

class ReportTypes {
public:
    // { Post: no day is done yet }
    ReportTypes(const hms::behaviour::Clock& clock, const hms::behaviour::HistoryReader& reader,
                hms::behaviour::History& history, const hms::behaviour::HeartbeatCounter& heartbeats,
                const hms::behaviour::AdcRegistry& registry, hms::behaviour::ReportSender& sender);

    // { Pre: now is hospital local time in seconds; call it regularly (every tick) }
    //   generateDaily(now)
    // { Post: the first matching row decides the result:
    //         1. today's report was already sent      => AlreadyDone, nothing happens
    //         2. now is before 06:00                  => TooEarly, nothing happens
    //         3. reading history or heartbeat counts
    //            fails                                => GenerationFailed, failure logged
    //         4. sending fails                        => SendFailed, failure logged
    //            (rows 3 and 4 leave the day not done, so the next call retries)
    //         5. otherwise                            => Sent: one report for [now - 24 h, now)
    //                                                    was sent; today is marked done }
    DailyVerdict generateDaily(long long now);

    // { Pre: none }
    //   summary(user, filters)
    // { Post: the first matching row decides the verdict:
    //         1. user is not Pharmacist or Administrator  => NotAuthorized
    //         2. filters.from > filters.to                => InvalidRange
    //         3. reading history fails                    => ReadFailure
    //         4. otherwise                                => Summarized: every entry in
    //                                                        [from, to] matching all filters
    //                                                        (possibly none)
    //         Then exactly one audit entry is written. If that write fails, the result
    //         is AuditFailure with no entries, whatever the verdict was. }
    SummaryResult summary(const hms::behaviour::User& user, const SummaryFilters& filters);

private:
    bool buildDaily(long long now, DailyReport& report) const;
    void logFailure(long long now, const std::string& what);
    bool audit(const hms::behaviour::User& user, const SummaryFilters& filters, SummaryVerdict verdict);

    const hms::behaviour::Clock& clock;
    const hms::behaviour::HistoryReader& reader;
    hms::behaviour::History& history;
    const hms::behaviour::HeartbeatCounter& heartbeats;
    const hms::behaviour::AdcRegistry& registry;
    hms::behaviour::ReportSender& sender;
    std::optional<long long> lastReportDay;   // the local day whose report was sent
};
