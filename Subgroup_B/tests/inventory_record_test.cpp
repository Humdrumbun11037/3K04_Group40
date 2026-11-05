// Tests for the inventory record: addItem, the withdraw(), deposit() and
// manualChange() function tables row by row, low-stock and discrepancy alarms,
// and the invariants.
#include <cassert>
#include <cstdio>

#include "behaviour_hiding/inventory_record.hpp"
#include "tests/fakes/fake_alarm_notifier.hpp"
#include "tests/fakes/fake_clock.hpp"
#include "tests/fakes/fake_permissions.hpp"
#include "tests/fakes/fake_history.hpp"

using namespace hms::behaviour;

static const User technician{"Tara", Role::Technician};
static const User pharmacist{"Priya", Role::Pharmacist};
static const User administrator{"Alex", Role::Administrator};

// Clock at t = 500. ADC-1 holds 40 of 100 Morphine.
struct Setup {
    FakeClock clock{500};
    FakeHistory history;
    FakeAlarmNotifier notifier;
    FakePermissions permissions;
    InventoryRecord inventory{clock, history, notifier, permissions};
    Setup() {
        assert(inventory.addItem(1, "Morphine", 40, 100) == AddItemVerdict::Added);
    }
    int morphine() { return *inventory.count(1, "Morphine"); }
};

// --- addItem ---

static void add_item_bad_values_rejected() {
    Setup s;
    assert(s.inventory.addItem(1, "Insulin", -1, 100) == AddItemVerdict::IncorrectValue);
    assert(s.inventory.addItem(1, "Insulin", 10, 0) == AddItemVerdict::IncorrectValue);
    assert(!s.inventory.count(1, "Insulin").has_value());
}

static void add_item_twice_changes_nothing() {
    Setup s;
    assert(s.inventory.addItem(1, "Morphine", 5, 100) == AddItemVerdict::AlreadyExists);
    assert(s.morphine() == 40);
}

static void add_item_same_medication_other_adc_is_separate() {
    Setup s;
    assert(s.inventory.addItem(2, "Morphine", 90, 100) == AddItemVerdict::Added);
    assert(s.morphine() == 40);
    assert(*s.inventory.count(2, "Morphine") == 90);
}

// --- withdraw() function table ---

static void withdraw_row1_unknown_item() {
    Setup s;
    assert(s.inventory.withdraw(1, "Insulin", 1) == InventoryVerdict::UnknownItem);
    assert(s.inventory.withdraw(9, "Morphine", 1) == InventoryVerdict::UnknownItem);
}

static void withdraw_row2_negative_amount_rejected() {
    Setup s;
    assert(s.inventory.withdraw(1, "Morphine", -5) == InventoryVerdict::IncorrectValue);
    assert(s.morphine() == 40);
    assert(s.history.entries.empty() && s.notifier.raised.empty());
}

static void withdraw_row2_zero_amount_rejected() {
    Setup s;
    assert(s.inventory.withdraw(1, "Morphine", 0) == InventoryVerdict::IncorrectValue);
    assert(s.morphine() == 40 && s.history.entries.empty());
}

static void withdraw_row3_history_failure_changes_nothing() {
    Setup s;
    s.history.failWrites(true);
    assert(s.inventory.withdraw(1, "Morphine", 5) == InventoryVerdict::StorageFailure);
    assert(s.morphine() == 40);
}

// The ADC has already dispensed 50, but the HMS thought it held 40: the
// withdrawal is recorded, the count goes to 0, and a discrepancy alarm is raised.
static void withdraw_row4_more_than_count_recorded_as_discrepancy() {
    Setup s;
    assert(s.inventory.withdraw(1, "Morphine", 50) == InventoryVerdict::DiscrepancyRecorded);
    assert(s.morphine() == 0);
    assert(s.history.entries.size() == 1);
    const HistoryEntry& e = s.history.entries[0];
    assert(e.kind == "InventoryDiscrepancy" && e.who == "ADC-1");
    assert(e.detail == "withdrew 50 Morphine: 40 -> 0 (discrepancy: HMS count was 40)");
    assert(e.adc == 1 && e.medication == "Morphine");
}

static void withdraw_row4_discrepancy_alarm_then_low_stock_alarm() {
    Setup s;
    s.inventory.withdraw(1, "Morphine", 41);
    assert(s.notifier.raised.size() == 2);
    assert(s.notifier.raised[0].adc == 1);
    assert(s.notifier.raised[0].type == AlarmType::InventoryDiscrepancy);
    assert(s.notifier.raised[0].detail == "Morphine");
    assert(s.notifier.raised[1].type == AlarmType::InventoryLow);   // count is now 0
}

static void withdraw_row4_history_failure_checked_first() {
    Setup s;
    s.history.failWrites(true);
    assert(s.inventory.withdraw(1, "Morphine", 50) == InventoryVerdict::StorageFailure);
    assert(s.morphine() == 40 && s.notifier.raised.empty());
}

static void withdraw_row5_count_reduced_and_recorded() {
    Setup s;
    assert(s.inventory.withdraw(1, "Morphine", 5) == InventoryVerdict::Updated);
    assert(s.morphine() == 35);
    assert(s.history.entries.size() == 1);
    const HistoryEntry& e = s.history.entries[0];
    assert(e.kind == "InventoryWithdrawal");
    assert(e.who == "ADC-1");
    assert(e.detail == "withdrew 5 Morphine: 40 -> 35");
    assert(e.time == 500);
    assert(e.adc == 1);
    assert(e.medication == "Morphine");
}

static void withdraw_row5_whole_count_leaves_zero_without_discrepancy() {
    Setup s;
    assert(s.inventory.withdraw(1, "Morphine", 40) == InventoryVerdict::Updated);
    assert(s.morphine() == 0);
    assert(s.notifier.raised.size() == 1 && s.notifier.raised[0].type == AlarmType::InventoryLow);
}

// --- deposit() function table ---

static void deposit_row1_unknown_item() {
    Setup s;
    assert(s.inventory.deposit(1, "Insulin", 5) == InventoryVerdict::UnknownItem);
    assert(s.inventory.deposit(9, "Morphine", 5) == InventoryVerdict::UnknownItem);
    assert(s.history.entries.empty());
}

static void deposit_row2_zero_or_negative_rejected() {
    Setup s;
    assert(s.inventory.deposit(1, "Morphine", 0) == InventoryVerdict::IncorrectValue);
    assert(s.inventory.deposit(1, "Morphine", -5) == InventoryVerdict::IncorrectValue);
    assert(s.morphine() == 40 && s.history.entries.empty());
}

static void deposit_row2_count_too_large_rejected() {
    Setup s;
    assert(s.inventory.deposit(1, "Morphine", 2147483647) == InventoryVerdict::IncorrectValue);
    assert(s.morphine() == 40);
}

static void deposit_row3_history_failure_changes_nothing() {
    Setup s;
    s.history.failWrites(true);
    assert(s.inventory.deposit(1, "Morphine", 5) == InventoryVerdict::StorageFailure);
    assert(s.morphine() == 40);
}

static void deposit_row4_count_increased_and_recorded() {
    Setup s;
    assert(s.inventory.deposit(1, "Morphine", 25) == InventoryVerdict::Updated);
    assert(s.morphine() == 65);
    assert(s.history.entries.size() == 1);
    const HistoryEntry& e = s.history.entries[0];
    assert(e.kind == "InventoryDeposit" && e.who == "ADC-1" && e.time == 500);
    assert(e.detail == "deposited 25 Morphine: 40 -> 65");
    assert(e.adc == 1 && e.medication == "Morphine");
    assert(s.notifier.raised.empty());
}

static void deposit_row4_above_capacity_allowed() {
    Setup s;
    assert(s.inventory.deposit(1, "Morphine", 100) == InventoryVerdict::Updated);
    assert(s.morphine() == 140);   // capacity is 100
}

// --- manualChange() function table ---

static void manual_row1_technician_not_authorized() {
    Setup s;
    assert(s.inventory.manualChange(technician, 1, "Morphine", 50, "recount")
           == InventoryVerdict::NotAuthorized);
    assert(s.morphine() == 40);
}

static void manual_row2_unknown_item() {
    Setup s;
    assert(s.inventory.manualChange(pharmacist, 1, "Insulin", 50, "recount")
           == InventoryVerdict::UnknownItem);
}

static void manual_row3_negative_count_rejected() {
    Setup s;
    assert(s.inventory.manualChange(pharmacist, 1, "Morphine", -1, "recount")
           == InventoryVerdict::IncorrectValue);
    assert(s.morphine() == 40);
}

static void manual_row4_no_explanation_rejected() {
    Setup s;
    assert(s.inventory.manualChange(pharmacist, 1, "Morphine", 50, "")
           == InventoryVerdict::NoExplanation);
    assert(s.inventory.manualChange(pharmacist, 1, "Morphine", 50, "  ")
           == InventoryVerdict::NoExplanation);
    assert(s.morphine() == 40);
}

static void manual_row5_history_failure_changes_nothing() {
    Setup s;
    s.history.failWrites(true);
    assert(s.inventory.manualChange(pharmacist, 1, "Morphine", 50, "recount")
           == InventoryVerdict::StorageFailure);
    assert(s.morphine() == 40);
}

static void manual_row6_pharmacist_updates_with_explanation() {
    Setup s;
    assert(s.inventory.manualChange(pharmacist, 1, "Morphine", 50, "restocked")
           == InventoryVerdict::Updated);
    assert(s.morphine() == 50);
    assert(s.history.entries.size() == 1);
    assert(s.history.entries[0].kind == "InventoryManualChange");
    assert(s.history.entries[0].who == "Priya");
    assert(s.history.entries[0].detail == "Morphine: 40 -> 50: restocked");
}

static void manual_row6_administrator_may_update() {
    Setup s;
    assert(s.inventory.manualChange(administrator, 1, "Morphine", 60, "recount")
           == InventoryVerdict::Updated);
}

static void manual_row6_zero_allowed() {
    Setup s;
    assert(s.inventory.manualChange(pharmacist, 1, "Morphine", 0, "expired, discarded")
           == InventoryVerdict::Updated);
    assert(s.morphine() == 0);
}

static void manual_rows_checked_in_order() {
    Setup s;   // breaks rows 1, 3 and 4 at once: row 1 wins
    assert(s.inventory.manualChange(technician, 1, "Morphine", -1, "")
           == InventoryVerdict::NotAuthorized);
}

// --- low-stock alarms (uses the low stock rule) ---

static void low_stock_alarm_when_below_25_percent() {
    Setup s;
    s.inventory.withdraw(1, "Morphine", 16);   // 24 of 100
    assert(s.notifier.raised.size() == 1);
    assert(s.notifier.raised[0].adc == 1);
    assert(s.notifier.raised[0].type == AlarmType::InventoryLow);
    assert(s.notifier.raised[0].detail == "Morphine");
}

static void low_stock_alarm_detail_is_the_medication() {
    Setup s;
    s.inventory.addItem(1, "Insulin", 50, 100);
    s.inventory.withdraw(1, "Morphine", 40);
    s.inventory.withdraw(1, "Insulin", 49);
    assert(s.notifier.raised.size() == 2);
    assert(s.notifier.raised[0].detail == "Morphine");
    assert(s.notifier.raised[1].detail == "Insulin");
}

static void no_alarm_at_exactly_25_percent() {
    Setup s;
    s.inventory.withdraw(1, "Morphine", 15);   // 25 of 100
    assert(s.notifier.raised.empty());
}

static void low_stock_alarm_after_manual_change() {
    Setup s;
    s.inventory.manualChange(pharmacist, 1, "Morphine", 0, "expired, discarded");
    assert(s.notifier.raised.size() == 1);
}

static void no_alarm_when_change_rejected() {
    Setup s;
    s.history.failWrites(true);
    s.inventory.withdraw(1, "Morphine", 30);
    assert(s.notifier.raised.empty());
}

// --- invariants ---

static void invariant_count_never_negative() {
    Setup s;
    s.inventory.manualChange(pharmacist, 1, "Morphine", -10, "oops");   // rejected
    assert(s.morphine() == 40);
    s.inventory.withdraw(1, "Morphine", 100);                          // discrepancy: stops at 0
    assert(s.morphine() == 0);
}

static void items_lists_every_item() {
    Setup s;
    s.inventory.addItem(1, "Insulin", 10, 20);
    std::vector<StockItem> all = s.inventory.items();
    assert(all.size() == 2);
    assert(all[0].medication == "Insulin" && all[0].count == 10 && all[0].capacity == 20);
    assert(all[1].medication == "Morphine" && all[1].count == 40);
}

// --- added: ADC ids must be positive; the permission table decides row 1 ---

static void add_item_adc_must_be_positive() {
    Setup s;
    assert(s.inventory.addItem(0, "Insulin", 10, 100) == AddItemVerdict::IncorrectValue);
    assert(s.inventory.addItem(-3, "Insulin", 10, 100) == AddItemVerdict::IncorrectValue);
}

static void manual_row1_follows_the_permission_table() {
    Setup s;
    s.permissions.set(Role::Pharmacist, Action::ManualInventoryChange, false);
    s.permissions.set(Role::Technician, Action::ManualInventoryChange, true);
    assert(s.inventory.manualChange(pharmacist, 1, "Morphine", 50, "recount")
           == InventoryVerdict::NotAuthorized);
    assert(s.inventory.manualChange(technician, 1, "Morphine", 50, "recount")
           == InventoryVerdict::Updated);
}

int main() {
    add_item_bad_values_rejected();
    add_item_twice_changes_nothing();
    add_item_same_medication_other_adc_is_separate();
    withdraw_row1_unknown_item();
    withdraw_row2_negative_amount_rejected();
    withdraw_row2_zero_amount_rejected();
    withdraw_row3_history_failure_changes_nothing();
    withdraw_row4_more_than_count_recorded_as_discrepancy();
    withdraw_row4_discrepancy_alarm_then_low_stock_alarm();
    withdraw_row4_history_failure_checked_first();
    withdraw_row5_count_reduced_and_recorded();
    withdraw_row5_whole_count_leaves_zero_without_discrepancy();
    deposit_row1_unknown_item();
    deposit_row2_zero_or_negative_rejected();
    deposit_row2_count_too_large_rejected();
    deposit_row3_history_failure_changes_nothing();
    deposit_row4_count_increased_and_recorded();
    deposit_row4_above_capacity_allowed();
    manual_row1_technician_not_authorized();
    manual_row2_unknown_item();
    manual_row3_negative_count_rejected();
    manual_row4_no_explanation_rejected();
    manual_row5_history_failure_changes_nothing();
    manual_row6_pharmacist_updates_with_explanation();
    manual_row6_administrator_may_update();
    manual_row6_zero_allowed();
    manual_rows_checked_in_order();
    low_stock_alarm_when_below_25_percent();
    low_stock_alarm_detail_is_the_medication();
    no_alarm_at_exactly_25_percent();
    low_stock_alarm_after_manual_change();
    no_alarm_when_change_rejected();
    invariant_count_never_negative();
    items_lists_every_item();
    add_item_adc_must_be_positive();
    manual_row1_follows_the_permission_table();
    std::printf("inventory_record_test: 36 tests passed\n");
    return 0;
}
