//gui world

/*
Hides: how the GUI wires the nine real behaviour-hiding modules to the fakes
       that stand in for teammates' modules, and how the simulated ADCs and
       clock behave (one heartbeat a minute per ADC that is not silenced;
       periodic checks every 10 simulated seconds)

Interface
takes: simulation commands (advance the clock, silence or resume an ADC,
       add or decommission an ADC, make history or report sending fail)
sends: the modules themselves, for the API to call, and a feed of what
       happened (alarms, escalations, status changes, daily reports)
uses: every behaviour module; the fakes in tests/fakes/

Real: alarm severity, alarm resolution, report types, report formatting,
inventory record, low stock rule, hospital stock, ADC heartbeats, maintenance.
Fake: clock, history, ADC registry, heartbeat counter, report sender,
permissions (the agreed table), alarm notification.
Same starting state as demo_main.cpp, so the demo and the GUI agree.
*/
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "behaviour_hiding/adc_heartbeats.hpp"
#include "behaviour_hiding/alarm_resolution.hpp"
#include "behaviour_hiding/hospital_stock.hpp"
#include "behaviour_hiding/inventory_record.hpp"
#include "behaviour_hiding/maintenance.hpp"
#include "behaviour_hiding/report_formatting.hpp"
#include "behaviour_hiding/report_types.hpp"
#include "tests/fakes/fake_adc_registry.hpp"
#include "tests/fakes/fake_clock.hpp"
#include "tests/fakes/fake_heartbeat_counter.hpp"
#include "tests/fakes/fake_history.hpp"
#include "tests/fakes/fake_permissions.hpp"
#include "tests/fakes/fake_report_sender.hpp"

// One thing that happened, for the page's activity feed.
struct FeedItem {
    long long time;
    std::string kind;   // "alarm", "escalation", "status", "report", "action" or "scenario"
    std::string text;
};

// A maintenance window the GUI scheduled. Maintenance has no query that lists
// its windows, so the GUI keeps its own copy for display; Maintenance still
// decides every verdict and whether an ADC is in maintenance.
struct WindowView {
    hms::behaviour::AdcId adc;
    long long start;
    long long end;
    bool cancelled;
};

// Stand-in for Inodh's alarm notification: hands every raised alarm to the
// real alarm resolution, and puts it in the feed. Choosing the destination
// role is notification's job; this is the same guess the console demo makes.
class GuiNotifier : public hms::behaviour::AlarmNotifier {
public:
    GuiNotifier(AlarmResolution& alarms, std::vector<FeedItem>& feed, const hms::behaviour::Clock& clock)
        : alarms(alarms), feed(feed), clock(clock) {}

    void raise(const hms::behaviour::AdcId& adc, AlarmType type, const std::string& detail = "") override;

private:
    static hms::behaviour::Role destinationFor(AlarmType type);

    AlarmResolution& alarms;
    std::vector<FeedItem>& feed;
    const hms::behaviour::Clock& clock;
};

class GuiWorld {
public:
    static constexpr long long START_TIME = 1793598000;   // 2026-11-02 05:40:00 local
    static constexpr long long TICK_SECONDS = 10;          // periodic checks run this often
    static constexpr long long HEARTBEAT_SECONDS = 60;     // simulated ADCs beat this often
    static constexpr long long MAX_ADVANCE = 2 * 24 * 60 * 60;

    // Two online ADCs with a day of heartbeats behind them, some stock and reserves.
    GuiWorld();
    GuiWorld(const GuiWorld&) = delete;
    GuiWorld& operator=(const GuiWorld&) = delete;

    // Registers and starts monitoring an ADC.
    void addAdc(const hms::behaviour::AdcId& adc);
    // Removes an ADC from the registry and stops monitoring it, so it can't alarm.
    void decommission(const hms::behaviour::AdcId& adc);
    // One heartbeat from a simulated ADC, logged so the daily report can count it.
    void sendHeartbeat(const hms::behaviour::AdcId& adc);
    // Moves the clock forward in 10 s steps; ADCs that are not silenced beat once a
    // minute, and the periodic checks run at every step.
    void advance(long long seconds);
    // What the HMS main loop would run every tick.
    void periodicChecks();
    // Runs the daily report; returns its verdict and adds it to the feed.
    DailyVerdict runDaily();

    void setHistoryFailing(bool failing);
    void setSenderFailing(bool failing);
    void note(const std::string& kind, const std::string& text);
    std::string timeText(long long seconds) const { return format.formatTime(seconds); }

    // Fakes and real modules, in construction order.
    FakeClock clock{START_TIME};
    FakeHistory history;
    FakeAdcRegistry registry;
    FakeHeartbeatCounter heartbeatLog;
    FakeReportSender sender;
    ReportFormatting format;
    FakePermissions permissions;   // the team's agreed table only
    std::vector<FeedItem> feed;
    AlarmResolution alarms{clock, history, permissions};
    GuiNotifier notifier{alarms, feed, clock};
    Maintenance maintenance{clock, registry, history, permissions};
    AdcHeartbeats heartbeats{clock, maintenance, notifier, history};
    InventoryRecord inventory{clock, history, notifier, permissions};
    HospitalStock hospital{clock, inventory, history, notifier, permissions};
    ReportTypes reports{clock, history, history, heartbeatLog, registry, sender, permissions};

    hms::behaviour::User user{"Tara", hms::behaviour::Role::Technician};
    std::set<hms::behaviour::AdcId> silenced;
    std::map<hms::behaviour::AdcId, long long> lastHeartbeat;
    std::vector<WindowView> windows;
    bool historyFailing = false;
    bool senderFailing = false;
    DailyVerdict lastDaily = DailyVerdict::TooEarly;

private:
    void noteStatusChanges();
    std::map<hms::behaviour::AdcId, AdcStatus> shownStatus;
};
