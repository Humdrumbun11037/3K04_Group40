// Tests for ADC heartbeats: the offline table row by row (spec rows 1-4,
// cpp rows A-D), the invariants, the AdcHealth history entries, then one
// end-to-end run with the real maintenance module.
#include <cassert>
#include <cstdio>

#include "behaviour_hiding/adc_heartbeats.hpp"
#include "behaviour_hiding/maintenance.hpp"
#include "tests/fakes/fake_adc_registry.hpp"
#include "tests/fakes/fake_alarm_notifier.hpp"
#include "tests/fakes/fake_clock.hpp"
#include "tests/fakes/fake_permissions.hpp"
#include "tests/fakes/fake_history.hpp"
#include "tests/fakes/fake_maintenance_status.hpp"

using namespace hms::behaviour;

// Clock starts at t = 0. Only ADC-1 is monitored.
struct Setup {
    FakeClock clock{0};
    FakeMaintenanceStatus maintenance;
    FakeAlarmNotifier notifier;
    FakeHistory history;
    AdcHeartbeats heartbeats{clock, maintenance, notifier, history};
    Setup() {
        heartbeats.monitor(1);
    }
};

// --- spec row 1: valid update received ---

static void row1_heartbeat_resets_missed_count() {
    Setup s;
    s.clock.advance(170);
    assert(s.heartbeats.heartbeatReceived(1) == HeartbeatVerdict::Accepted);
    s.clock.advance(170);   // 340 s since start, but only 170 s since the heartbeat
    s.heartbeats.checkAll();
    assert(s.heartbeats.status(1) == AdcStatus::Online);
    assert(s.notifier.raised.empty());
}

static void row1_heartbeat_brings_offline_adc_back_online() {
    Setup s;
    s.clock.advance(180);
    s.heartbeats.checkAll();
    assert(s.heartbeats.status(1) == AdcStatus::Offline);
    s.heartbeats.heartbeatReceived(1);
    assert(s.heartbeats.status(1) == AdcStatus::Online);
}

static void row1_heartbeat_from_unmonitored_adc_rejected() {
    Setup s;
    assert(s.heartbeats.heartbeatReceived(9) == HeartbeatVerdict::NotMonitored);
    assert(s.heartbeats.status(9) == AdcStatus::NotMonitored);
}

// --- spec row 2: 3 missed periods, not in maintenance (cpp rows A and D) ---

static void row2_two_missed_periods_still_online() {
    Setup s;   // cpp row A
    s.clock.advance(179);
    s.heartbeats.checkAll();
    assert(s.heartbeats.status(1) == AdcStatus::Online);
    assert(s.notifier.raised.empty());
}

static void row2_three_missed_periods_offline_and_alarm() {
    Setup s;   // cpp row D
    s.heartbeats.monitor(2);
    s.clock.advance(180);
    s.heartbeats.heartbeatReceived(2);   // only ADC-1 stays silent
    s.heartbeats.checkAll();
    assert(s.heartbeats.status(1) == AdcStatus::Offline);
    assert(s.heartbeats.status(2) == AdcStatus::Online);
    assert(s.notifier.raised.size() == 1);
    assert(s.notifier.raised[0].adc == 1);
    assert(s.notifier.raised[0].type == AlarmType::AdcOffline);
}

// --- spec row 3: communication lost during maintenance (cpp row B) ---

static void row3_silence_during_maintenance_offline_no_alarm() {
    Setup s;
    s.maintenance.start(1);
    s.clock.advance(600);
    s.heartbeats.checkAll();
    assert(s.heartbeats.status(1) == AdcStatus::Offline);
    assert(s.notifier.raised.empty());
}

static void row3_heartbeat_during_maintenance_back_online() {
    Setup s;
    s.maintenance.start(1);
    s.clock.advance(600);
    s.heartbeats.checkAll();
    s.heartbeats.heartbeatReceived(1);
    s.maintenance.end(1);
    s.heartbeats.checkAll();
    assert(s.heartbeats.status(1) == AdcStatus::Online);
    assert(s.notifier.raised.empty());
}

// --- spec row 4: maintenance ends and the ADC is still silent (cpp row D) ---

static void row4_maintenance_ends_still_silent_offline_and_alarm() {
    Setup s;
    s.maintenance.start(1);
    s.clock.advance(600);
    s.heartbeats.checkAll();
    assert(s.notifier.raised.empty());
    s.maintenance.end(1);
    s.heartbeats.checkAll();
    assert(s.heartbeats.status(1) == AdcStatus::Offline);
    assert(s.notifier.raised.size() == 1);
    assert(s.notifier.raised[0].type == AlarmType::AdcOffline);
}

// --- invariants ---

static void invariant_no_second_alarm_while_offline() {
    Setup s;   // cpp row C
    s.clock.advance(180);
    s.heartbeats.checkAll();
    s.clock.advance(600);
    s.heartbeats.checkAll();
    s.heartbeats.checkAll();
    assert(s.notifier.raised.size() == 1);
}

static void invariant_alarm_before_maintenance_stays_open() {
    Setup s;
    s.clock.advance(180);
    s.heartbeats.checkAll();                    // offline, one alarm
    s.maintenance.start(1);               // window begins while offline
    s.clock.advance(600);
    s.heartbeats.checkAll();
    assert(s.heartbeats.status(1) == AdcStatus::Offline);
    s.maintenance.end(1);
    s.heartbeats.checkAll();
    assert(s.notifier.raised.size() == 1);      // not withdrawn, not repeated
}

static void invariant_new_offline_spell_raises_new_alarm() {
    Setup s;
    s.clock.advance(180);
    s.heartbeats.checkAll();
    s.heartbeats.heartbeatReceived(1);    // back online
    s.clock.advance(180);
    s.heartbeats.checkAll();                    // silent again
    assert(s.notifier.raised.size() == 2);
}

static void invariant_decommissioned_adc_never_alarms() {
    Setup s;
    s.heartbeats.stopMonitoring(1);
    s.clock.advance(1000);
    s.heartbeats.checkAll();
    assert(s.notifier.raised.empty());
    assert(s.heartbeats.status(1) == AdcStatus::NotMonitored);
}

static void monitor_twice_changes_nothing() {
    Setup s;
    s.clock.advance(120);
    s.heartbeats.monitor(1);              // must not reset the silence
    s.clock.advance(60);
    s.heartbeats.checkAll();
    assert(s.heartbeats.status(1) == AdcStatus::Offline);
}

// --- AdcHealth history entries (never block a change or an alarm) ---

static void health_offline_recorded() {
    Setup s;
    s.clock.advance(180);
    s.heartbeats.checkAll();
    assert(s.history.entries.size() == 1);
    const HistoryEntry& e = s.history.entries[0];
    assert(e.kind == "AdcHealth");
    assert(e.who == "HMS");
    assert(e.adc == 1);
    assert(e.detail == "ADC-1 offline: 3 heartbeat periods missed");
    assert(e.time == 180);
}

static void health_back_online_recorded() {
    Setup s;
    s.clock.advance(180);
    s.heartbeats.checkAll();
    s.clock.advance(5);
    s.heartbeats.heartbeatReceived(1);
    assert(s.history.entries.size() == 2);
    assert(s.history.entries[1].detail == "ADC-1 back online");
    assert(s.history.entries[1].time == 185);
}

static void health_offline_in_maintenance_recorded_once() {
    Setup s;
    s.maintenance.start(1);
    s.clock.advance(600);
    s.heartbeats.checkAll();
    assert(s.history.entries.size() == 1);
    assert(s.history.entries[0].detail == "ADC-1 offline during maintenance: no alarm");
    s.maintenance.end(1);
    s.heartbeats.checkAll();                    // alarm now, but already Offline
    assert(s.notifier.raised.size() == 1);
    assert(s.history.entries.size() == 1);
}

static void health_nothing_recorded_without_a_change() {
    Setup s;
    s.heartbeats.heartbeatReceived(1);    // Online -> Online
    s.clock.advance(180);
    s.heartbeats.checkAll();
    s.heartbeats.checkAll();                    // Offline -> Offline
    assert(s.history.entries.size() == 1);
}

static void health_history_failure_never_blocks() {
    Setup s;
    s.history.failWrites(true);
    s.clock.advance(180);
    s.heartbeats.checkAll();
    assert(s.heartbeats.status(1) == AdcStatus::Offline);
    assert(s.notifier.raised.size() == 1);
    s.heartbeats.heartbeatReceived(1);
    assert(s.heartbeats.status(1) == AdcStatus::Online);
}

// --- end to end with the real maintenance module ---

static void real_maintenance_window_suppresses_then_releases() {
    FakeClock clock{0};
    FakeAdcRegistry registry;
    registry.add(1);
    FakeHistory history;
    FakePermissions permissions;
    Maintenance maintenance{clock, registry, history, permissions};
    FakeAlarmNotifier notifier;
    AdcHeartbeats heartbeats{clock, maintenance, notifier, history};
    heartbeats.monitor(1);

    User technician{"Tara", Role::Technician};
    assert(maintenance.schedule(technician, 1, 0, 600) == ScheduleVerdict::Created);

    clock.advance(300);                         // silent, but in maintenance
    heartbeats.checkAll();
    assert(notifier.raised.empty());

    clock.advance(300);                         // t = 600: window over
    heartbeats.checkAll();
    assert(heartbeats.status(1) == AdcStatus::Offline);
    assert(notifier.raised.size() == 1);
}

int main() {
    row1_heartbeat_resets_missed_count();
    row1_heartbeat_brings_offline_adc_back_online();
    row1_heartbeat_from_unmonitored_adc_rejected();
    row2_two_missed_periods_still_online();
    row2_three_missed_periods_offline_and_alarm();
    row3_silence_during_maintenance_offline_no_alarm();
    row3_heartbeat_during_maintenance_back_online();
    row4_maintenance_ends_still_silent_offline_and_alarm();
    invariant_no_second_alarm_while_offline();
    invariant_alarm_before_maintenance_stays_open();
    invariant_new_offline_spell_raises_new_alarm();
    invariant_decommissioned_adc_never_alarms();
    monitor_twice_changes_nothing();
    health_offline_recorded();
    health_back_online_recorded();
    health_offline_in_maintenance_recorded_once();
    health_nothing_recorded_without_a_change();
    health_history_failure_never_blocks();
    real_maintenance_window_suppresses_then_releases();
    std::printf("adc_heartbeats_test: 19 tests passed\n");
    return 0;
}
