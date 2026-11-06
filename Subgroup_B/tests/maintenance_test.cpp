// Tests for maintenance: the schedule() and cancel() function tables row by row,
// the window lifecycle, the no-overlap invariant, and the history entries.
#include <cassert>
#include <cstdio>

#include "behaviour_hiding/maintenance.hpp"
#include "tests/fakes/fake_adc_registry.hpp"
#include "tests/fakes/fake_clock.hpp"
#include "tests/fakes/fake_permissions.hpp"
#include "tests/fakes/fake_history.hpp"

using namespace hms::behaviour;

static const User technician{"Tara", Role::Technician};
static const User pharmacist{"Priya", Role::Pharmacist};
static const User administrator{"Alex", Role::Administrator};

// Clock starts at t = 1000. ADC-1 and ADC-2 are registered.
struct Setup {
    FakeClock clock{1000};
    FakeAdcRegistry registry;
    FakeHistory history;
    FakePermissions permissions;
    Maintenance maintenance{clock, registry, history, permissions};
    Setup() {
        registry.add(1);
        registry.add(2);
    }
};

// --- schedule() function table ---

static void schedule_row1_pharmacist_not_authorized() {
    Setup s;
    assert(s.maintenance.schedule(pharmacist, 1, 2000, 3000) == ScheduleVerdict::NotAuthorized);
}

static void schedule_row1_administrator_not_authorized() {
    Setup s;
    assert(s.maintenance.schedule(administrator, 1, 2000, 3000) == ScheduleVerdict::NotAuthorized);
}

static void schedule_row2_unregistered_adc_rejected() {
    Setup s;
    assert(s.maintenance.schedule(technician, 9, 2000, 3000) == ScheduleVerdict::UnknownAdc);
}

static void schedule_row2_decommissioned_adc_rejected() {
    Setup s;
    s.registry.decommission(1);
    assert(s.maintenance.schedule(technician, 1, 2000, 3000) == ScheduleVerdict::UnknownAdc);
}

static void schedule_row3_end_before_start_rejected() {
    Setup s;
    assert(s.maintenance.schedule(technician, 1, 3000, 2000) == ScheduleVerdict::InvalidWindow);
}

static void schedule_row3_end_equal_to_start_rejected() {
    Setup s;
    assert(s.maintenance.schedule(technician, 1, 2000, 2000) == ScheduleVerdict::InvalidWindow);
}

static void schedule_row3_end_not_after_now_rejected() {
    Setup s;   // now = 1000
    assert(s.maintenance.schedule(technician, 1, 500, 1000) == ScheduleVerdict::InvalidWindow);
}

static void schedule_row5_overlapping_window_rejected() {
    Setup s;
    assert(s.maintenance.schedule(technician, 1, 2000, 3000) == ScheduleVerdict::Created);
    assert(s.maintenance.schedule(technician, 1, 2000, 3000) == ScheduleVerdict::Overlap); // same
    assert(s.maintenance.schedule(technician, 1, 2200, 2800) == ScheduleVerdict::Overlap); // inside
    assert(s.maintenance.schedule(technician, 1, 1500, 2500) == ScheduleVerdict::Overlap); // left edge
    assert(s.maintenance.schedule(technician, 1, 2500, 3500) == ScheduleVerdict::Overlap); // right edge
    assert(s.maintenance.schedule(technician, 1, 1500, 3500) == ScheduleVerdict::Overlap); // around
}

static void schedule_row6_valid_window_created() {
    Setup s;
    assert(s.maintenance.schedule(technician, 1, 2000, 3000) == ScheduleVerdict::Created);
    assert(s.maintenance.isInMaintenance(1, 2000));
}

static void schedule_row6_window_already_under_way_created() {
    Setup s;   // now = 1000: start in the past is allowed, end must be in the future
    assert(s.maintenance.schedule(technician, 1, 500, 2000) == ScheduleVerdict::Created);
    assert(s.maintenance.isInMaintenance(1, 1000));
}

static void schedule_rows_checked_in_order() {
    Setup s;   // breaks rows 1, 2 and 3 at once: row 1 wins
    assert(s.maintenance.schedule(pharmacist, 9, 3000, 2000) == ScheduleVerdict::NotAuthorized);
}

static void schedule_rejected_request_changes_nothing() {
    Setup s;
    s.maintenance.schedule(pharmacist, 1, 2000, 3000);
    assert(!s.maintenance.isInMaintenance(1, 2500));
}

// --- cancel() function table ---

static void cancel_row1_non_technician_rejected() {
    Setup s;
    s.maintenance.schedule(technician, 1, 2000, 3000);
    assert(s.maintenance.cancel(pharmacist, 1, 2000) == CancelVerdict::NotAuthorized);
    assert(s.maintenance.cancel(administrator, 1, 2000) == CancelVerdict::NotAuthorized);
    assert(s.maintenance.isInMaintenance(1, 2500));   // still there
}

static void cancel_row2_unknown_window_rejected() {
    Setup s;
    s.maintenance.schedule(technician, 1, 2000, 3000);
    assert(s.maintenance.cancel(technician, 1, 2100) == CancelVerdict::UnknownWindow); // wrong start
    assert(s.maintenance.cancel(technician, 2, 2000) == CancelVerdict::UnknownWindow); // wrong ADC
}

static void cancel_row3_completed_window_not_cancellable() {
    Setup s;
    s.maintenance.schedule(technician, 1, 2000, 3000);
    s.clock.set(3000);
    assert(s.maintenance.cancel(technician, 1, 2000) == CancelVerdict::NotCancellable);
}

static void cancel_row3_cancelled_window_not_cancellable() {
    Setup s;
    s.maintenance.schedule(technician, 1, 2000, 3000);
    assert(s.maintenance.cancel(technician, 1, 2000) == CancelVerdict::Cancelled);
    assert(s.maintenance.cancel(technician, 1, 2000) == CancelVerdict::NotCancellable);
}

static void cancel_row4_scheduled_window_cancelled() {
    Setup s;
    s.maintenance.schedule(technician, 1, 2000, 3000);
    assert(s.maintenance.cancel(technician, 1, 2000) == CancelVerdict::Cancelled);
    assert(!s.maintenance.isInMaintenance(1, 2500));   // never becomes Active
}

static void cancel_row4_active_window_ends_suppression_immediately() {
    Setup s;
    s.maintenance.schedule(technician, 1, 2000, 3000);
    s.clock.set(2500);
    assert(s.maintenance.isInMaintenance(1, 2500));
    assert(s.maintenance.cancel(technician, 1, 2000) == CancelVerdict::Cancelled);
    assert(!s.maintenance.isInMaintenance(1, 2500));
}

static void cancel_rows_checked_in_order() {
    Setup s;   // breaks rows 1 and 2 at once: row 1 wins
    assert(s.maintenance.cancel(pharmacist, 9, 2000) == CancelVerdict::NotAuthorized);
}

// --- window lifecycle: Scheduled -> Active -> Completed ---

static void state_scheduled_is_not_maintenance() {
    Setup s;
    s.maintenance.schedule(technician, 1, 2000, 3000);
    assert(!s.maintenance.isInMaintenance(1, 1999));
}

static void state_active_from_start_until_end() {
    Setup s;
    s.maintenance.schedule(technician, 1, 2000, 3000);
    assert(s.maintenance.isInMaintenance(1, 2000));
    assert(s.maintenance.isInMaintenance(1, 2999));
}

static void state_completed_at_end() {
    Setup s;
    s.maintenance.schedule(technician, 1, 2000, 3000);
    assert(s.maintenance.isInMaintenance(1, 2500));
    assert(!s.maintenance.isInMaintenance(1, 3000));
}

static void state_whole_window_passed_unseen_is_completed() {
    Setup s;
    s.maintenance.schedule(technician, 1, 2000, 3000);
    assert(!s.maintenance.isInMaintenance(1, 5000));
}

// --- invariant: no two open windows for one ADC overlap ---

static void invariant_back_to_back_windows_allowed() {
    Setup s;
    assert(s.maintenance.schedule(technician, 1, 2000, 3000) == ScheduleVerdict::Created);
    assert(s.maintenance.schedule(technician, 1, 3000, 4000) == ScheduleVerdict::Created);
    assert(s.maintenance.schedule(technician, 1, 1500, 2000) == ScheduleVerdict::Created);
}

static void invariant_overlap_is_per_adc() {
    Setup s;
    assert(s.maintenance.schedule(technician, 1, 2000, 3000) == ScheduleVerdict::Created);
    assert(s.maintenance.schedule(technician, 2, 2000, 3000) == ScheduleVerdict::Created);
}

static void invariant_completed_window_does_not_block() {
    Setup s;
    s.maintenance.schedule(technician, 1, 1000, 2000);
    s.clock.set(2500);   // that window is now Completed, though nobody asked
    assert(s.maintenance.schedule(technician, 1, 1500, 3000) == ScheduleVerdict::Created);
}

static void invariant_cancelled_window_does_not_block() {
    Setup s;
    s.maintenance.schedule(technician, 1, 2000, 3000);
    s.maintenance.cancel(technician, 1, 2000);
    assert(s.maintenance.schedule(technician, 1, 2000, 3000) == ScheduleVerdict::Created);
    // cancel now finds the new open window, not the old cancelled one
    assert(s.maintenance.cancel(technician, 1, 2000) == CancelVerdict::Cancelled);
}

static void invariant_other_adc_not_in_maintenance() {
    Setup s;
    s.maintenance.schedule(technician, 1, 2000, 3000);
    assert(!s.maintenance.isInMaintenance(2, 2500));
}

// --- history entries (never block a change) ---

static void history_schedule_recorded() {
    Setup s;
    s.maintenance.schedule(technician, 1, 2000, 3000);
    assert(s.history.entries.size() == 1);
    const HistoryEntry& e = s.history.entries[0];
    assert(e.kind == "Maintenance");
    assert(e.who == "Tara");
    assert(e.adc == 1);
    assert(e.detail == "ADC-1 window scheduled (start=2000 end=3000)");
    assert(e.time == 1000);
}

static void history_start_and_completion_recorded_at_window_times() {
    Setup s;
    s.maintenance.schedule(technician, 1, 2000, 3000);
    s.maintenance.isInMaintenance(1, 5000);   // both happened unseen
    assert(s.history.entries.size() == 3);
    assert(s.history.entries[1].detail == "ADC-1 window started (start=2000 end=3000)");
    assert(s.history.entries[1].time == 2000);
    assert(s.history.entries[1].who == "HMS");
    assert(s.history.entries[2].detail == "ADC-1 window completed (start=2000 end=3000)");
    assert(s.history.entries[2].time == 3000);
    s.maintenance.isInMaintenance(1, 6000);   // nothing new to record
    assert(s.history.entries.size() == 3);
}

static void history_cancel_recorded() {
    Setup s;
    s.maintenance.schedule(technician, 1, 2000, 3000);
    s.clock.set(1500);
    s.maintenance.cancel(technician, 1, 2000);
    assert(s.history.entries.size() == 2);
    assert(s.history.entries[1].detail == "ADC-1 window cancelled (start=2000 end=3000)");
    assert(s.history.entries[1].time == 1500);
    s.maintenance.isInMaintenance(1, 5000);   // a cancelled window never starts
    assert(s.history.entries.size() == 2);
}

static void history_rejected_request_writes_nothing() {
    Setup s;
    s.maintenance.schedule(pharmacist, 1, 2000, 3000);
    s.maintenance.cancel(technician, 1, 2000);
    assert(s.history.entries.empty());
}

static void history_failure_never_blocks_a_change() {
    Setup s;
    s.history.failWrites(true);
    assert(s.maintenance.schedule(technician, 1, 2000, 3000) == ScheduleVerdict::Created);
    assert(s.maintenance.isInMaintenance(1, 2500));
    s.clock.set(2500);
    assert(s.maintenance.cancel(technician, 1, 2000) == CancelVerdict::Cancelled);
    assert(!s.maintenance.isInMaintenance(1, 2500));
}

// --- added: 12-hour limit (schedule row 4) and the permission table (row 1) ---

static void schedule_row4_longer_than_12_hours_rejected() {
    Setup s;
    assert(s.maintenance.schedule(technician, 1, 2000, 2000 + 12 * 3600 + 1) == ScheduleVerdict::WindowTooLong);
    assert(!s.maintenance.isInMaintenance(1, 2500));
    assert(s.history.entries.empty());
}

static void schedule_row4_exactly_12_hours_allowed() {
    Setup s;
    assert(s.maintenance.schedule(technician, 1, 2000, 2000 + 12 * 3600) == ScheduleVerdict::Created);
}

static void schedule_row4_checked_before_overlap() {
    Setup s;   // too long and overlapping at once: row 4 wins
    s.maintenance.schedule(technician, 1, 2000, 3000);
    assert(s.maintenance.schedule(technician, 1, 2000, 2000 + 13 * 3600) == ScheduleVerdict::WindowTooLong);
}

static void schedule_row1_follows_the_permission_table() {
    Setup s;   // no role rule in the module: change the table and the answer changes
    s.permissions.set(Role::Administrator, Action::ScheduleMaintenance, true);
    assert(s.maintenance.schedule(administrator, 1, 2000, 3000) == ScheduleVerdict::Created);
}

static void cancel_row1_follows_the_permission_table() {
    Setup s;
    s.maintenance.schedule(technician, 1, 2000, 3000);
    s.permissions.set(Role::Technician, Action::CancelMaintenance, false);
    assert(s.maintenance.cancel(technician, 1, 2000) == CancelVerdict::NotAuthorized);
}

int main() {
    schedule_row1_pharmacist_not_authorized();
    schedule_row1_administrator_not_authorized();
    schedule_row2_unregistered_adc_rejected();
    schedule_row2_decommissioned_adc_rejected();
    schedule_row3_end_before_start_rejected();
    schedule_row3_end_equal_to_start_rejected();
    schedule_row3_end_not_after_now_rejected();
    schedule_row5_overlapping_window_rejected();
    schedule_row6_valid_window_created();
    schedule_row6_window_already_under_way_created();
    schedule_rows_checked_in_order();
    schedule_rejected_request_changes_nothing();
    cancel_row1_non_technician_rejected();
    cancel_row2_unknown_window_rejected();
    cancel_row3_completed_window_not_cancellable();
    cancel_row3_cancelled_window_not_cancellable();
    cancel_row4_scheduled_window_cancelled();
    cancel_row4_active_window_ends_suppression_immediately();
    cancel_rows_checked_in_order();
    state_scheduled_is_not_maintenance();
    state_active_from_start_until_end();
    state_completed_at_end();
    state_whole_window_passed_unseen_is_completed();
    invariant_back_to_back_windows_allowed();
    invariant_overlap_is_per_adc();
    invariant_completed_window_does_not_block();
    invariant_cancelled_window_does_not_block();
    invariant_other_adc_not_in_maintenance();
    history_schedule_recorded();
    history_start_and_completion_recorded_at_window_times();
    history_cancel_recorded();
    history_rejected_request_writes_nothing();
    history_failure_never_blocks_a_change();
    schedule_row4_longer_than_12_hours_rejected();
    schedule_row4_exactly_12_hours_allowed();
    schedule_row4_checked_before_overlap();
    schedule_row1_follows_the_permission_table();
    cancel_row1_follows_the_permission_table();
    std::printf("maintenance_test: 38 tests passed\n");
    return 0;
}
