// Tests for report types: the generateDaily() and summary() function tables
// row by row, the section and attention rules, and the invariants.
#include <cassert>
#include <cstdio>
#include <string>

#include "behaviour_hiding/report_types.hpp"
#include "tests/fakes/fake_adc_registry.hpp"
#include "tests/fakes/fake_clock.hpp"
#include "tests/fakes/fake_permissions.hpp"
#include "tests/fakes/fake_heartbeat_counter.hpp"
#include "tests/fakes/fake_history.hpp"
#include "tests/fakes/fake_report_sender.hpp"

using namespace hms::behaviour;

static const User technician{"Tara", Role::Technician};
static const User pharmacist{"Priya", Role::Pharmacist};
static const User administrator{"Alex", Role::Administrator};

static const long long DAY = 86400;
static const long long MIDNIGHT = 20759 * DAY;        // 2026-11-02 00:00:00
static const long long SIX_AM = MIDNIGHT + 6 * 3600;  // 2026-11-02 06:00:00

struct Setup {
    FakeClock clock{SIX_AM};
    FakeHistory history;
    FakeHeartbeatCounter counter;
    FakeAdcRegistry registry;
    FakeReportSender sender;
    FakePermissions permissions;
    ReportTypes reports{clock, history, history, counter, registry, sender, permissions};

    void write(const std::string& kind, const std::string& who, long long time,
               AdcId adc = 0, const std::string& medication = "") {
        history.record({kind, who, kind + " detail", time, adc, medication});
    }
    // Every heartbeat an ADC should send in the 24 hours before t.
    void healthyHeartbeats(const AdcId& adc, long long t) {
        for (long long time = t - DAY; time < t; time += 60) counter.record(adc, time);
    }
};

// --- generateDaily() function table ---

static void daily_row1_already_sent_today_no_action() {
    Setup s;
    assert(s.reports.generateDaily(SIX_AM) == DailyVerdict::Sent);
    assert(s.reports.generateDaily(SIX_AM + 3600) == DailyVerdict::AlreadyDone);
    assert(s.reports.generateDaily(MIDNIGHT + DAY - 1) == DailyVerdict::AlreadyDone);
    assert(s.sender.sent.size() == 1);
}

static void daily_row2_before_six_too_early() {
    Setup s;
    assert(s.reports.generateDaily(SIX_AM - 1) == DailyVerdict::TooEarly);
    assert(s.reports.generateDaily(MIDNIGHT) == DailyVerdict::TooEarly);
    assert(s.sender.sent.empty());
    assert(s.history.entries.empty());
}

static void daily_row2_next_day_before_six_too_early() {
    Setup s;
    s.reports.generateDaily(SIX_AM);
    assert(s.reports.generateDaily(MIDNIGHT + DAY + 100) == DailyVerdict::TooEarly);
}

static void daily_row3_history_read_failure_logged_then_retried() {
    Setup s;
    s.history.failReads(true);
    assert(s.reports.generateDaily(SIX_AM) == DailyVerdict::GenerationFailed);
    assert(s.sender.sent.empty());
    assert(s.history.entries.size() == 1);
    assert(s.history.entries[0].kind == "ReportFailed");
    assert(s.history.entries[0].detail == "daily report could not be generated");
    s.history.failReads(false);
    assert(s.reports.generateDaily(SIX_AM + 10) == DailyVerdict::Sent);
}

static void daily_row3_heartbeat_count_failure() {
    Setup s;
    s.registry.add(1);
    s.counter.failCounts(true);
    assert(s.reports.generateDaily(SIX_AM) == DailyVerdict::GenerationFailed);
    assert(s.sender.sent.empty());
}

static void daily_row4_send_failure_logged_then_retried() {
    Setup s;
    s.sender.failSends(true);
    assert(s.reports.generateDaily(SIX_AM) == DailyVerdict::SendFailed);
    assert(s.history.entries.size() == 1);
    assert(s.history.entries[0].detail == "daily report could not be sent");
    s.sender.failSends(false);
    assert(s.reports.generateDaily(SIX_AM + 10) == DailyVerdict::Sent);
    assert(s.sender.sent.size() == 1);
}

static void daily_row5_window_is_last_24_hours() {
    Setup s;
    s.reports.generateDaily(SIX_AM + 30);
    const DailyReport& r = s.sender.sent[0];
    assert(r.from == SIX_AM + 30 - DAY);
    assert(r.to == SIX_AM + 30);
}

static void daily_row5_next_day_sends_again() {
    Setup s;
    assert(s.reports.generateDaily(SIX_AM) == DailyVerdict::Sent);
    assert(s.reports.generateDaily(SIX_AM + DAY) == DailyVerdict::Sent);
    assert(s.sender.sent.size() == 2);
}

static void daily_row5_empty_window_gives_empty_sections() {
    Setup s;
    assert(s.reports.generateDaily(SIX_AM) == DailyVerdict::Sent);
    const DailyReport& r = s.sender.sent[0];
    assert(r.adcsNeedingAttention.empty() && r.inventoryChanges.empty() && r.events.empty()
           && r.alarms.empty() && r.adcHealth.empty());
}

static void daily_row5_reserve_change_is_an_inventory_change() {
    Setup s;
    s.write("ReserveChange", "Priya", SIX_AM - 100, 0, "Insulin");
    s.reports.generateDaily(SIX_AM);
    const DailyReport& r = s.sender.sent[0];
    assert(r.inventoryChanges.size() == 1 && r.inventoryChanges[0].kind == "ReserveChange");
    assert(r.events.empty());
}

static void daily_row5_entries_sorted_into_sections() {
    Setup s;
    s.write("InventoryWithdrawal", "ADC-1", SIX_AM - 100);
    s.write("AlarmResolved", "Tara", SIX_AM - 90);
    s.write("AdcHealth", "HMS", SIX_AM - 80, 1);
    s.write("Maintenance", "Tara", SIX_AM - 70, 2);
    s.write("SummaryRequested", "Alex", SIX_AM - 60);
    s.reports.generateDaily(SIX_AM);
    const DailyReport& r = s.sender.sent[0];
    assert(r.inventoryChanges.size() == 1 && r.inventoryChanges[0].kind == "InventoryWithdrawal");
    assert(r.alarms.size() == 1 && r.alarms[0].kind == "AlarmResolved");
    assert(r.adcHealth.size() == 2);
    assert(r.adcHealth[0].kind == "AdcHealth" && r.adcHealth[1].kind == "Maintenance");
    assert(r.events.size() == 1 && r.events[0].kind == "SummaryRequested");
}

static void daily_row5_only_entries_inside_window() {
    Setup s;
    s.write("AlarmResolved", "Tara", SIX_AM - DAY - 1);   // just before the window
    s.write("AlarmResolved", "Tara", SIX_AM - DAY);       // first second of the window
    s.write("AlarmResolved", "Tara", SIX_AM);             // the window excludes its end
    s.reports.generateDaily(SIX_AM);
    assert(s.sender.sent[0].alarms.size() == 1);
    assert(s.sender.sent[0].alarms[0].time == SIX_AM - DAY);
}

static void daily_row5_flags_adc_with_too_few_heartbeats() {
    Setup s;
    s.registry.add(1);
    s.registry.add(2);
    s.healthyHeartbeats(1, SIX_AM);
    for (int i = 0; i < 10; i++) s.counter.record(2, SIX_AM - 600 + i * 60);
    s.reports.generateDaily(SIX_AM);
    const DailyReport& r = s.sender.sent[0];
    assert(r.adcsNeedingAttention.size() == 1);
    assert(r.adcsNeedingAttention[0].adc == 2);
    assert(r.adcsNeedingAttention[0].received == 10);
    assert(r.adcsNeedingAttention[0].expected == 1440);
}

static void daily_decommissioned_adc_not_flagged() {
    Setup s;
    s.registry.add(1);
    s.registry.decommission(1);
    s.reports.generateDaily(SIX_AM);
    assert(s.sender.sent[0].adcsNeedingAttention.empty());
}

static void invariant_daily_generation_is_read_only() {
    Setup s;
    s.write("AlarmResolved", "Tara", SIX_AM - 100);
    s.reports.generateDaily(SIX_AM);
    assert(s.history.entries.size() == 1);   // nothing written on success
}

// --- summary() function table ---

static void summary_row1_technician_not_authorized() {
    Setup s;
    SummaryResult r = s.reports.summary(technician, {0, "", "", 0, SIX_AM});
    assert(r.verdict == SummaryVerdict::NotAuthorized);
    assert(r.summary.entries.empty());
    assert(s.history.entries.size() == 1);
    assert(s.history.entries[0].kind == "SummaryRequested");
    assert(s.history.entries[0].who == "Tara");
    assert(s.history.entries[0].detail.rfind("NotAuthorized ", 0) == 0);
}

static void summary_row2_from_after_to_invalid_range() {
    Setup s;
    SummaryResult r = s.reports.summary(pharmacist, {0, "", "", 200, 100});
    assert(r.verdict == SummaryVerdict::InvalidRange);
    assert(s.history.entries.size() == 1);
    assert(s.history.entries[0].detail.rfind("InvalidRange ", 0) == 0);
}

static void summary_row3_read_failure() {
    Setup s;
    s.history.failReads(true);
    SummaryResult r = s.reports.summary(pharmacist, {0, "", "", 0, SIX_AM});
    assert(r.verdict == SummaryVerdict::ReadFailure);
    assert(s.history.entries.size() == 1);   // still audited
}

static void summary_row4_no_matches_gives_empty_summary() {
    Setup s;
    s.write("AlarmResolved", "Tara", 100, 1);
    SummaryResult r = s.reports.summary(pharmacist, {2, "", "", 0, 1000});
    assert(r.verdict == SummaryVerdict::Summarized);
    assert(r.summary.entries.empty());
    assert(s.history.entries.size() == 2);   // the entry plus the audit
}

static void summary_row4_matches_each_filter() {
    Setup s;
    s.write("InventoryWithdrawal", "ADC-1", 100, 1, "Morphine");
    s.write("InventoryWithdrawal", "ADC-2", 110, 2, "Morphine");
    s.write("InventoryManualChange", "Priya", 120, 1, "Insulin");
    s.write("AlarmResolved", "Tara", 130, 1);
    assert(s.reports.summary(pharmacist, {1, "", "", 0, 1000}).summary.entries.size() == 3);
    assert(s.reports.summary(pharmacist, {0, "Morphine", "", 0, 1000}).summary.entries.size() == 2);
    assert(s.reports.summary(pharmacist, {0, "", "Tara", 0, 1000}).summary.entries.size() == 1);
    SummaryResult all = s.reports.summary(administrator, {1, "Insulin", "Priya", 0, 1000});
    assert(all.verdict == SummaryVerdict::Summarized);
    assert(all.summary.entries.size() == 1);
    assert(all.summary.entries[0].kind == "InventoryManualChange");
    assert(all.summary.filters.medication == "Insulin");
}

static void summary_row4_window_includes_both_ends() {
    Setup s;
    s.write("AlarmResolved", "Tara", 99);
    s.write("AlarmResolved", "Tara", 100);
    s.write("AlarmResolved", "Tara", 200);
    s.write("AlarmResolved", "Tara", 201);
    assert(s.reports.summary(pharmacist, {0, "", "", 100, 200}).summary.entries.size() == 2);
    assert(s.reports.summary(pharmacist, {0, "", "", 100, 100}).summary.entries.size() == 1);
}

static void summary_audit_failure_returns_no_summary() {
    Setup s;
    s.write("AlarmResolved", "Tara", 100);
    s.history.failWrites(true);
    SummaryResult r = s.reports.summary(pharmacist, {0, "", "", 0, 1000});
    assert(r.verdict == SummaryVerdict::AuditFailure);
    assert(r.summary.entries.empty());
}

static void summary_audit_failure_on_rejected_request() {
    Setup s;
    s.history.failWrites(true);
    assert(s.reports.summary(technician, {0, "", "", 0, 1000}).verdict == SummaryVerdict::AuditFailure);
}

static void invariant_exactly_one_audit_per_request() {
    Setup s;
    s.reports.summary(technician, {0, "", "", 0, 1000});
    s.reports.summary(pharmacist, {0, "", "", 1000, 0});
    s.reports.summary(pharmacist, {0, "", "", 0, 1000});
    assert(s.history.entries.size() == 3);
    for (const HistoryEntry& e : s.history.entries) assert(e.kind == "SummaryRequested");
}

static void audit_entry_records_filters() {
    Setup s;
    s.reports.summary(pharmacist, {1, "", "Tara", 5, 10});
    assert(s.history.entries[0].detail
           == "Summarized adc=ADC-1 medication=any actor=Tara from=5 to=10");
    assert(s.history.entries[0].time == SIX_AM);
}

// --- added: the permission table decides summary row 1 ---

static void summary_row1_follows_the_permission_table() {
    Setup s;
    s.permissions.set(Role::Technician, Action::GenerateSummary, true);
    assert(s.reports.summary(technician, {0, "", "", 0, 1000}).verdict == SummaryVerdict::Summarized);
}

int main() {
    daily_row1_already_sent_today_no_action();
    daily_row2_before_six_too_early();
    daily_row2_next_day_before_six_too_early();
    daily_row3_history_read_failure_logged_then_retried();
    daily_row3_heartbeat_count_failure();
    daily_row4_send_failure_logged_then_retried();
    daily_row5_window_is_last_24_hours();
    daily_row5_next_day_sends_again();
    daily_row5_empty_window_gives_empty_sections();
    daily_row5_entries_sorted_into_sections();
    daily_row5_reserve_change_is_an_inventory_change();
    daily_row5_only_entries_inside_window();
    daily_row5_flags_adc_with_too_few_heartbeats();
    daily_decommissioned_adc_not_flagged();
    invariant_daily_generation_is_read_only();
    summary_row1_technician_not_authorized();
    summary_row2_from_after_to_invalid_range();
    summary_row3_read_failure();
    summary_row4_no_matches_gives_empty_summary();
    summary_row4_matches_each_filter();
    summary_row4_window_includes_both_ends();
    summary_audit_failure_returns_no_summary();
    summary_audit_failure_on_rejected_request();
    invariant_exactly_one_audit_per_request();
    audit_entry_records_filters();
    summary_row1_follows_the_permission_table();
    std::printf("report_types_test: 26 tests passed\n");
    return 0;
}
