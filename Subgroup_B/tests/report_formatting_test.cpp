// Tests for report formatting: hand-made dates checked against values from the
// system `date` tool, and exact output for the daily report and summary.
#include <cassert>
#include <cstdio>
#include <string>

#include "behaviour_hiding/report_formatting.hpp"

using namespace hms::behaviour;

static const long long MIDNIGHT = 20759LL * 86400;   // 2026-11-02 00:00:00
static const long long SIX_AM = MIDNIGHT + 6 * 3600;

// --- formatTime ---

static void time_epoch_start() {
    ReportFormatting f;
    assert(f.formatTime(0) == "1970-01-01 00:00:00");
}

static void time_demo_day() {
    ReportFormatting f;
    assert(f.formatTime(MIDNIGHT) == "2026-11-02 00:00:00");
    assert(f.formatTime(1793598900) == "2026-11-02 05:55:00");
}

static void time_leap_day() {
    ReportFormatting f;
    assert(f.formatTime(1709210096) == "2024-02-29 12:34:56");
}

static void time_year_boundary() {
    ReportFormatting f;
    assert(f.formatTime(1767225599) == "2025-12-31 23:59:59");
    assert(f.formatTime(1767225600) == "2026-01-01 00:00:00");
}

static void time_century_leap_year() {
    ReportFormatting f;   // 2000 is a leap year (divisible by 400), so Feb has 29 days
    assert(f.formatTime(951868800) == "2000-03-01 00:00:00");
}

static void time_before_1970() {
    ReportFormatting f;
    assert(f.formatTime(-1) == "1969-12-31 23:59:59");
}

// --- formatDaily ---

static void daily_empty_report() {
    ReportFormatting f;
    DailyReport r{SIX_AM - 86400, SIX_AM, {}, {}, {}, {}, {}};
    assert(f.formatDaily(r) ==
        "DAILY REPORT\n"
        "Window: 2026-11-01 06:00:00 to 2026-11-02 06:00:00\n"
        "\n"
        "ADCs needing attention (0)\n"
        "  (none)\n"
        "Inventory changes (0)\n"
        "  (none)\n"
        "Events (0)\n"
        "  (none)\n"
        "Alarms (0)\n"
        "  (none)\n"
        "ADC health (0)\n"
        "  (none)\n");
}

static void daily_report_with_contents() {
    ReportFormatting f;
    DailyReport r{SIX_AM - 86400, SIX_AM, {}, {}, {}, {}, {}};
    r.adcsNeedingAttention.push_back({"ADC-2", 10, 1440});
    r.inventoryChanges.push_back({"InventoryWithdrawal", "ADC-1", "withdrew 5 Morphine: 40 -> 35",
                                  MIDNIGHT + 3600, "ADC-1", "Morphine"});
    r.alarms.push_back({"AlarmResolved", "Tara", "alarm 1: replaced cable", MIDNIGHT + 7200, "ADC-1", ""});
    r.alarms.push_back({"AlarmEscalated", "HMS", "alarm 2", MIDNIGHT + 7260, "ADC-2", ""});
    assert(f.formatDaily(r) ==
        "DAILY REPORT\n"
        "Window: 2026-11-01 06:00:00 to 2026-11-02 06:00:00\n"
        "\n"
        "ADCs needing attention (1)\n"
        "  ADC-2: 10 of 1440 expected heartbeats\n"
        "Inventory changes (1)\n"
        "  2026-11-02 01:00:00 ADC-1 InventoryWithdrawal withdrew 5 Morphine: 40 -> 35\n"
        "Events (0)\n"
        "  (none)\n"
        "Alarms (2)\n"
        "  2026-11-02 02:00:00 Tara AlarmResolved alarm 1: replaced cable\n"
        "  2026-11-02 02:01:00 HMS AlarmEscalated alarm 2\n"
        "ADC health (0)\n"
        "  (none)\n");
}

// --- formatSummary ---

static void summary_empty() {
    ReportFormatting f;
    Summary s{{"", "", "", MIDNIGHT, SIX_AM}, {}};
    assert(f.formatSummary(s) ==
        "ACTIVITY SUMMARY\n"
        "Filters: adc=any medication=any actor=any from=2026-11-02 00:00:00 to=2026-11-02 06:00:00\n"
        "Entries: 0\n");
}

static void summary_with_entries() {
    ReportFormatting f;
    Summary s{{"ADC-1", "Morphine", "Priya", MIDNIGHT, SIX_AM}, {}};
    s.entries.push_back({"InventoryManualChange", "Priya", "Morphine: 35 -> 50: restocked",
                         MIDNIGHT + 300, "ADC-1", "Morphine"});
    s.entries.push_back({"InventoryManualChange", "Priya", "Morphine: 50 -> 48: recount",
                         MIDNIGHT + 330, "ADC-1", "Morphine"});
    assert(f.formatSummary(s) ==
        "ACTIVITY SUMMARY\n"
        "Filters: adc=ADC-1 medication=Morphine actor=Priya from=2026-11-02 00:00:00 to=2026-11-02 06:00:00\n"
        "Entries: 2\n"
        "2026-11-02 00:05:00 Priya InventoryManualChange Morphine: 35 -> 50: restocked\n"
        "2026-11-02 00:05:30 Priya InventoryManualChange Morphine: 50 -> 48: recount\n");
}

int main() {
    time_epoch_start();
    time_demo_day();
    time_leap_day();
    time_year_boundary();
    time_century_leap_year();
    time_before_1970();
    daily_empty_report();
    daily_report_with_contents();
    summary_empty();
    summary_with_entries();
    std::printf("report_formatting_test: 10 tests passed\n");
    return 0;
}
