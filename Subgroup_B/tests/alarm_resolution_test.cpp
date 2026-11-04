// Tests for alarm resolution: the add(), acknowledge() and resolve() function
// tables row by row, the escalation table, and the invariants.
#include <cassert>
#include <cstdio>

#include "behaviour_hiding/alarm_resolution.hpp"
#include "tests/fakes/fake_clock.hpp"
#include "tests/fakes/fake_history.hpp"

using namespace hms::behaviour;

static const User technician{"Tara", Role::Technician};
static const User pharmacist{"Priya", Role::Pharmacist};
static const User administrator{"Alex", Role::Administrator};
static const User unlistedRole{"Nobody", static_cast<Role>(99)};

// Clock starts at t = 0.
struct Setup {
    FakeClock clock{0};
    FakeHistory history;
    AlarmResolution alarms{clock, history};

    AlarmId add(AlarmType type, Role destination, const AdcId& adc = "ADC-1") {
        AddResult r = alarms.add(adc, type, destination);
        assert(r.verdict == AddVerdict::Added);
        return r.id;
    }
    AlarmState state(AlarmId id) { return alarms.find(id)->state; }
};

// --- add() function table ---

static void add_row1_unknown_type_rejected() {
    Setup s;
    AddResult r = s.alarms.add("ADC-1", static_cast<AlarmType>(99), Role::Technician);
    assert(r.verdict == AddVerdict::UnknownType);
    assert(!s.alarms.find(1).has_value());
}

static void add_row2_same_adc_and_type_joins_open_alarm() {
    Setup s;
    AlarmId id = s.add(AlarmType::AdcOffline, Role::Technician);
    AddResult again = s.alarms.add("ADC-1", AlarmType::AdcOffline, Role::Technician);
    assert(again.verdict == AddVerdict::JoinedOpenAlarm);
    assert(again.id == id);
}

static void add_row2_acknowledged_alarm_is_still_open() {
    Setup s;
    AlarmId id = s.add(AlarmType::AdcOffline, Role::Technician);
    s.alarms.acknowledge(technician, id);
    AddResult again = s.alarms.add("ADC-1", AlarmType::AdcOffline, Role::Technician);
    assert(again.verdict == AddVerdict::JoinedOpenAlarm);
    assert(again.id == id);
}

static void add_row2_same_detail_joins() {
    Setup s;
    AddResult first = s.alarms.add("ADC-1", AlarmType::InventoryLow, Role::Pharmacist, "Morphine");
    AddResult again = s.alarms.add("ADC-1", AlarmType::InventoryLow, Role::Pharmacist, "Morphine");
    assert(again.verdict == AddVerdict::JoinedOpenAlarm);
    assert(again.id == first.id);
    assert(s.alarms.find(first.id)->detail == "Morphine");
}

static void add_row3_different_detail_is_new() {
    Setup s;
    AddResult morphine = s.alarms.add("ADC-1", AlarmType::InventoryLow, Role::Pharmacist, "Morphine");
    AddResult insulin = s.alarms.add("ADC-1", AlarmType::InventoryLow, Role::Pharmacist, "Insulin");
    AddResult none = s.alarms.add("ADC-1", AlarmType::InventoryLow, Role::Pharmacist);
    assert(insulin.verdict == AddVerdict::Added && none.verdict == AddVerdict::Added);
    assert(morphine.id != insulin.id && insulin.id != none.id && morphine.id != none.id);
    assert(s.alarms.find(none.id)->detail.empty());
}

static void add_row3_resolved_alarm_not_joined() {
    Setup s;
    AlarmId id = s.add(AlarmType::AdcOffline, Role::Technician);
    s.alarms.resolve(technician, id, "fixed");
    AddResult again = s.alarms.add("ADC-1", AlarmType::AdcOffline, Role::Technician);
    assert(again.verdict == AddVerdict::Added);
    assert(again.id != id);
}

static void add_row3_other_adc_or_type_is_new() {
    Setup s;
    AlarmId first = s.add(AlarmType::AdcOffline, Role::Technician, "ADC-1");
    AlarmId otherAdc = s.add(AlarmType::AdcOffline, Role::Technician, "ADC-2");
    AlarmId otherType = s.add(AlarmType::ConnectionLoss, Role::Technician, "ADC-1");
    assert(first != otherAdc && first != otherType && otherAdc != otherType);
}

static void add_row3_takes_severity_from_table() {
    Setup s;
    AlarmId id = s.add(AlarmType::AdcOffline, Role::Technician);
    AlarmInfo info = *s.alarms.find(id);
    assert(info.severity == Severity::Critical);
    assert(info.state == AlarmState::Raised);
    assert(info.destination == Role::Technician);
    assert(!info.escalated);
}

// --- acknowledge() function table ---

static void ack_row1_unlisted_role_not_authorized() {
    Setup s;   // every real role may handle alarms, so only a forced value reaches row 1
    AlarmId id = s.add(AlarmType::AdcOffline, Role::Technician);
    assert(s.alarms.acknowledge(unlistedRole, id) == AcknowledgeVerdict::NotAuthorized);
    assert(s.state(id) == AlarmState::Raised);
}

static void ack_row2_unknown_alarm() {
    Setup s;
    assert(s.alarms.acknowledge(technician, 42) == AcknowledgeVerdict::UnknownAlarm);
}

static void ack_row3_already_acknowledged_not_raised() {
    Setup s;
    AlarmId id = s.add(AlarmType::AdcOffline, Role::Technician);
    s.alarms.acknowledge(technician, id);
    assert(s.alarms.acknowledge(technician, id) == AcknowledgeVerdict::NotRaised);
    assert(s.history.entries.size() == 1);   // the second attempt recorded nothing
}

static void ack_row3_resolved_not_raised() {
    Setup s;
    AlarmId id = s.add(AlarmType::AdcOffline, Role::Technician);
    s.alarms.resolve(technician, id, "fixed");
    assert(s.alarms.acknowledge(technician, id) == AcknowledgeVerdict::NotRaised);
    assert(s.state(id) == AlarmState::Resolved);
}

static void ack_row4_history_failure_stays_raised() {
    Setup s;
    AlarmId id = s.add(AlarmType::AdcOffline, Role::Technician);
    s.history.failWrites(true);
    assert(s.alarms.acknowledge(technician, id) == AcknowledgeVerdict::StorageFailure);
    assert(s.state(id) == AlarmState::Raised);
}

static void ack_row5_acknowledged_and_recorded() {
    Setup s;
    AlarmId id = s.add(AlarmType::AdcOffline, Role::Technician);
    s.clock.set(30);
    assert(s.alarms.acknowledge(technician, id) == AcknowledgeVerdict::Acknowledged);
    assert(s.state(id) == AlarmState::Acknowledged);
    assert(s.history.entries.size() == 1);
    assert(s.history.entries[0].kind == "AlarmAcknowledged");
    assert(s.history.entries[0].who == "Tara");
    assert(s.history.entries[0].time == 30);
}

// --- resolve() function table ---

static void resolve_row1_unlisted_role_not_authorized() {
    Setup s;
    AlarmId id = s.add(AlarmType::AdcOffline, Role::Technician);
    assert(s.alarms.resolve(unlistedRole, id, "fixed") == ResolveVerdict::NotAuthorized);
    assert(s.state(id) == AlarmState::Raised);
}

static void resolve_row1_all_three_roles_may_resolve() {
    Setup s;
    AlarmId a = s.add(AlarmType::AdcOffline, Role::Technician, "ADC-1");
    AlarmId b = s.add(AlarmType::AdcOffline, Role::Technician, "ADC-2");
    AlarmId c = s.add(AlarmType::AdcOffline, Role::Technician, "ADC-3");
    assert(s.alarms.resolve(pharmacist, a, "fixed") == ResolveVerdict::Resolved);
    assert(s.alarms.resolve(technician, b, "fixed") == ResolveVerdict::Resolved);
    assert(s.alarms.resolve(administrator, c, "fixed") == ResolveVerdict::Resolved);
}

static void resolve_row2_unknown_alarm() {
    Setup s;
    assert(s.alarms.resolve(technician, 42, "fixed") == ResolveVerdict::UnknownAlarm);
}

static void resolve_row3_already_resolved_changes_nothing() {
    Setup s;
    AlarmId id = s.add(AlarmType::AdcOffline, Role::Technician);
    assert(s.alarms.resolve(technician, id, "fixed") == ResolveVerdict::Resolved);
    assert(s.alarms.resolve(administrator, id, "again") == ResolveVerdict::AlreadyResolved);
    assert(s.state(id) == AlarmState::Resolved);
    assert(s.history.entries.size() == 1);   // only the first resolution
}

static void resolve_row4_no_explanation_rejected() {
    Setup s;
    AlarmId id = s.add(AlarmType::AdcOffline, Role::Technician);
    assert(s.alarms.resolve(technician, id, "") == ResolveVerdict::NoExplanation);
    assert(s.state(id) == AlarmState::Raised);
    assert(s.history.entries.empty());
}

static void resolve_row4_whitespace_note_rejected() {
    Setup s;
    AlarmId id = s.add(AlarmType::AdcOffline, Role::Technician);
    assert(s.alarms.resolve(technician, id, "   	 ") == ResolveVerdict::NoExplanation);
    assert(s.state(id) == AlarmState::Raised);
}

static void resolve_row5_history_failure_not_resolved() {
    Setup s;
    AlarmId id = s.add(AlarmType::AdcOffline, Role::Technician);
    s.history.failWrites(true);
    assert(s.alarms.resolve(technician, id, "fixed") == ResolveVerdict::StorageFailure);
    assert(s.state(id) == AlarmState::Raised);
    s.history.failWrites(false);              // storage back: the retry works
    assert(s.alarms.resolve(technician, id, "fixed") == ResolveVerdict::Resolved);
}

static void resolve_row6_resolved_with_who_and_why() {
    Setup s;
    AlarmId id = s.add(AlarmType::AdcOffline, Role::Technician);
    assert(s.alarms.resolve(technician, id, "replaced network cable") == ResolveVerdict::Resolved);
    assert(s.state(id) == AlarmState::Resolved);
    assert(s.history.entries.size() == 1);
    assert(s.history.entries[0].kind == "AlarmResolved");
    assert(s.history.entries[0].who == "Tara");
    assert(s.history.entries[0].detail == "alarm 1: replaced network cable");
    assert(s.history.entries[0].adc == "ADC-1");
}

static void resolve_row6_from_raised_counts_as_acknowledged() {
    Setup s;   // resolved straight from Raised: it can never escalate afterwards
    AlarmId id = s.add(AlarmType::AdcOffline, Role::Technician);
    assert(s.alarms.resolve(technician, id, "fixed") == ResolveVerdict::Resolved);
    assert(s.alarms.acknowledge(technician, id) == AcknowledgeVerdict::NotRaised);
    s.clock.set(120);
    assert(s.alarms.escalateDue().empty());
}

static void resolve_row6_after_acknowledge() {
    Setup s;
    AlarmId id = s.add(AlarmType::AdcOffline, Role::Technician);
    s.alarms.acknowledge(technician, id);
    assert(s.alarms.resolve(technician, id, "fixed") == ResolveVerdict::Resolved);
}

static void resolve_rows_checked_in_order() {
    Setup s;   // breaks rows 1, 2 and 4 at once: row 1 wins
    assert(s.alarms.resolve(unlistedRole, 42, "") == ResolveVerdict::NotAuthorized);
}

// --- escalation table and limits ---

static void escalate_row1_technician_rerouted_to_administrator() {
    Setup s;
    AlarmId id = s.add(AlarmType::AdcOffline, Role::Technician);   // Critical
    s.clock.set(119);
    assert(s.alarms.escalateDue().empty());
    s.clock.set(120);
    std::vector<Escalation> made = s.alarms.escalateDue();
    assert(made.size() == 1);
    assert(made[0].id == id);
    assert(made[0].destination == Role::Administrator);
    assert(made[0].severity == Severity::Critical);                // unchanged
}

static void escalate_row1_pharmacist_rerouted_to_administrator() {
    Setup s;
    AlarmId id = s.add(AlarmType::InventoryLow, Role::Pharmacist); // High
    s.clock.set(599);
    assert(s.alarms.escalateDue().empty());
    s.clock.set(600);
    s.alarms.escalateDue();
    assert(s.alarms.find(id)->destination == Role::Administrator);
    assert(s.alarms.find(id)->severity == Severity::High);
}

static void escalate_row2_administrator_high_becomes_critical() {
    Setup s;
    AlarmId id = s.add(AlarmType::InventoryLow, Role::Administrator);
    s.clock.set(600);
    s.alarms.escalateDue();
    assert(s.alarms.find(id)->destination == Role::Administrator);
    assert(s.alarms.find(id)->severity == Severity::Critical);
}

static void escalate_row3_administrator_critical_stays_critical() {
    Setup s;
    AlarmId id = s.add(AlarmType::AdcOffline, Role::Administrator);
    s.clock.set(120);
    assert(s.alarms.escalateDue().size() == 1);
    assert(s.alarms.find(id)->severity == Severity::Critical);
    assert(s.alarms.find(id)->escalated);
}

static void escalate_low_never() {
    Setup s;
    s.add(AlarmType::ConnectionLoss, Role::Technician);            // Low
    s.clock.set(1000000);
    assert(s.alarms.escalateDue().empty());
}

static void escalate_recorded_in_history() {
    Setup s;
    s.add(AlarmType::AdcOffline, Role::Technician);
    s.clock.set(120);
    s.alarms.escalateDue();
    assert(s.history.entries.size() == 1);
    assert(s.history.entries[0].kind == "AlarmEscalated");
    assert(s.history.entries[0].who == "HMS");
}

static void escalate_even_if_history_fails() {
    Setup s;
    AlarmId id = s.add(AlarmType::AdcOffline, Role::Technician);
    s.history.failWrites(true);
    s.clock.set(120);
    assert(s.alarms.escalateDue().size() == 1);
    assert(s.alarms.find(id)->destination == Role::Administrator);
}

// --- invariants ---

static void invariant_escalates_at_most_once() {
    Setup s;
    AlarmId id = s.add(AlarmType::InventoryLow, Role::Technician); // High
    s.clock.set(600);
    assert(s.alarms.escalateDue().size() == 1);                    // -> Administrator
    s.clock.set(6000);
    assert(s.alarms.escalateDue().empty());                        // not again
    assert(s.alarms.find(id)->severity == Severity::High);
}

static void invariant_acknowledged_alarm_never_escalates() {
    Setup s;
    AlarmId id = s.add(AlarmType::AdcOffline, Role::Technician);
    s.alarms.acknowledge(technician, id);
    s.clock.set(120);
    assert(s.alarms.escalateDue().empty());
}

static void invariant_resolved_alarm_never_escalates() {
    Setup s;
    AlarmId id = s.add(AlarmType::AdcOffline, Role::Technician);
    s.alarms.resolve(technician, id, "fixed");
    s.clock.set(120);
    assert(s.alarms.escalateDue().empty());
}

static void invariant_failed_acknowledge_still_escalates() {
    Setup s;
    AlarmId id = s.add(AlarmType::AdcOffline, Role::Technician);
    s.history.failWrites(true);
    s.alarms.acknowledge(technician, id);                          // StorageFailure
    s.clock.set(120);
    assert(s.alarms.escalateDue().size() == 1);
}

int main() {
    add_row1_unknown_type_rejected();
    add_row2_same_adc_and_type_joins_open_alarm();
    add_row2_acknowledged_alarm_is_still_open();
    add_row2_same_detail_joins();
    add_row3_different_detail_is_new();
    add_row3_resolved_alarm_not_joined();
    add_row3_other_adc_or_type_is_new();
    add_row3_takes_severity_from_table();
    ack_row1_unlisted_role_not_authorized();
    ack_row2_unknown_alarm();
    ack_row3_already_acknowledged_not_raised();
    ack_row3_resolved_not_raised();
    ack_row4_history_failure_stays_raised();
    ack_row5_acknowledged_and_recorded();
    resolve_row1_unlisted_role_not_authorized();
    resolve_row1_all_three_roles_may_resolve();
    resolve_row2_unknown_alarm();
    resolve_row3_already_resolved_changes_nothing();
    resolve_row4_no_explanation_rejected();
    resolve_row4_whitespace_note_rejected();
    resolve_row5_history_failure_not_resolved();
    resolve_row6_resolved_with_who_and_why();
    resolve_row6_from_raised_counts_as_acknowledged();
    resolve_row6_after_acknowledge();
    resolve_rows_checked_in_order();
    escalate_row1_technician_rerouted_to_administrator();
    escalate_row1_pharmacist_rerouted_to_administrator();
    escalate_row2_administrator_high_becomes_critical();
    escalate_row3_administrator_critical_stays_critical();
    escalate_low_never();
    escalate_recorded_in_history();
    escalate_even_if_history_fails();
    invariant_escalates_at_most_once();
    invariant_acknowledged_alarm_never_escalates();
    invariant_resolved_alarm_never_escalates();
    invariant_failed_acknowledge_still_escalates();
    std::printf("alarm_resolution_test: 36 tests passed\n");
    return 0;
}
