// Tests for hospital stock: addReserve, the check() and manualReserveChange()
// function tables row by row, how the hospital totals are built, how they
// differ from a single ADC, and when checkAll() raises HospitalLowStock.
#include <cassert>
#include <cstdio>

#include "behaviour_hiding/alarm_resolution.hpp"
#include "behaviour_hiding/hospital_stock.hpp"
#include "tests/fakes/fake_alarm_notifier.hpp"
#include "tests/fakes/fake_clock.hpp"
#include "tests/fakes/fake_history.hpp"
#include "tests/fakes/fake_permissions.hpp"

using namespace hms::behaviour;

static const User technician{"Tara", Role::Technician};
static const User pharmacist{"Priya", Role::Pharmacist};
static const User administrator{"Alex", Role::Administrator};

// ADC-1 holds 40 of 100 Morphine, ADC-2 holds 60 of 100 Morphine.
// The reserves hold 300 of 400 Morphine. Hospital Morphine: 400 of 600.
// Inventory record and hospital stock share one notifier, as in the HMS.
struct Setup {
    FakeClock clock{500};
    FakeHistory history;
    FakeAlarmNotifier notifier;
    FakePermissions permissions;
    InventoryRecord inventory{clock, history, notifier, permissions};
    HospitalStock hospital{clock, inventory, history, notifier, permissions};
    Setup() {
        assert(inventory.addItem(1, "Morphine", 40, 100) == AddItemVerdict::Added);
        assert(inventory.addItem(2, "Morphine", 60, 100) == AddItemVerdict::Added);
        assert(hospital.addReserve("Morphine", 300, 400) == AddItemVerdict::Added);
    }
    int reserve(const std::string& medication) { return hospital.check(medication).reserveCount; }
    // Alarms raised for the whole hospital (ADC id 0) and for single ADCs.
    int hospitalAlarms() { return countAlarms(true); }
    int adcAlarms() { return countAlarms(false); }
    int countAlarms(bool hospitalWide) {
        int n = 0;
        for (const auto& a : notifier.raised) {
            if ((a.adc == HospitalStock::HOSPITAL) == hospitalWide) n++;
        }
        return n;
    }
};

// --- addReserve ---

static void add_reserve_bad_values_rejected() {
    Setup s;
    assert(s.hospital.addReserve("Insulin", -1, 100) == AddItemVerdict::IncorrectValue);
    assert(s.hospital.addReserve("Insulin", 10, 0) == AddItemVerdict::IncorrectValue);
    assert(s.hospital.check("Insulin").verdict == HospitalStockVerdict::UnknownMedication);
}

static void add_reserve_twice_changes_nothing() {
    Setup s;
    assert(s.hospital.addReserve("Morphine", 5, 400) == AddItemVerdict::AlreadyExists);
    assert(s.reserve("Morphine") == 300);
}

// --- how the totals are built ---

static void totals_are_reserve_plus_every_adc() {
    Setup s;
    HospitalStockLevel m = s.hospital.check("Morphine");
    assert(m.medication == "Morphine");
    assert(m.reserveCount == 300 && m.reserveCapacity == 400);
    assert(m.adcCount == 100 && m.adcCapacity == 200);
    assert(m.totalCount == 400 && m.totalCapacity == 600);
}

static void other_medications_are_not_counted() {
    Setup s;
    assert(s.inventory.addItem(1, "Insulin", 90, 100) == AddItemVerdict::Added);
    assert(s.hospital.addReserve("Insulin", 500, 500) == AddItemVerdict::Added);
    HospitalStockLevel m = s.hospital.check("Morphine");
    assert(m.totalCount == 400 && m.totalCapacity == 600);
}

static void medication_with_no_reserve_uses_adcs_only() {
    Setup s;
    assert(s.inventory.addItem(1, "Insulin", 10, 100) == AddItemVerdict::Added);
    HospitalStockLevel i = s.hospital.check("Insulin");
    assert(i.reserveCount == 0 && i.reserveCapacity == 0);
    assert(i.totalCount == 10 && i.totalCapacity == 100);
    assert(i.verdict == HospitalStockVerdict::Low);
}

static void medication_only_in_reserves_is_counted() {
    Setup s;
    assert(s.hospital.addReserve("Heparin", 150, 200) == AddItemVerdict::Added);
    HospitalStockLevel h = s.hospital.check("Heparin");
    assert(h.adcCount == 0 && h.adcCapacity == 0);
    assert(h.totalCount == 150 && h.totalCapacity == 200);
    assert(h.verdict == HospitalStockVerdict::NotLow);
}

// --- check() function table ---

static void check_row1_unknown_medication() {
    Setup s;
    HospitalStockLevel x = s.hospital.check("Insulin");
    assert(x.verdict == HospitalStockVerdict::UnknownMedication);
    assert(x.totalCount == 0 && x.totalCapacity == 0);
}

static void check_row2_below_25_percent_of_hospital_capacity_is_low() {
    Setup s;
    assert(s.hospital.addReserve("Insulin", 20, 100) == AddItemVerdict::Added);
    assert(s.inventory.addItem(1, "Insulin", 29, 100) == AddItemVerdict::Added);
    assert(s.hospital.check("Insulin").verdict == HospitalStockVerdict::Low);   // 49 of 200
}

static void check_row3_exactly_25_percent_is_not_low() {
    Setup s;
    assert(s.hospital.addReserve("Insulin", 20, 100) == AddItemVerdict::Added);
    assert(s.inventory.addItem(1, "Insulin", 30, 100) == AddItemVerdict::Added);
    assert(s.hospital.check("Insulin").verdict == HospitalStockVerdict::NotLow); // 50 of 200
}

static void check_row3_well_stocked_is_not_low() {
    Setup s;
    assert(s.hospital.check("Morphine").verdict == HospitalStockVerdict::NotLow); // 400 of 600
}

// --- the hospital view differs from a single ADC ---

// ADC-1 drops to 10 of 100 Morphine: low for that ADC, so inventory record
// raises an ADC alarm. The hospital still holds 370 of 600, so checkAll()
// raises no hospital alarm.
static void one_adc_low_but_hospital_not_low() {
    Setup s;
    assert(s.inventory.withdraw(1, "Morphine", 30) == InventoryVerdict::Updated);
    HospitalStockLevel m = s.hospital.check("Morphine");
    assert(m.totalCount == 370 && m.totalCapacity == 600);
    assert(m.verdict == HospitalStockVerdict::NotLow);
    assert(s.hospital.checkAll().empty());
    assert(s.adcAlarms() == 1 && s.hospitalAlarms() == 0);
    assert(s.notifier.raised[0].adc == 1 && s.notifier.raised[0].type == AlarmType::InventoryLow);
}

// Every ADC is at or above 25%, but the reserves are nearly empty, so the
// hospital as a whole holds only 70 of 700 Insulin (10%). checkAll() raises a
// hospital alarm; no ADC alarm is raised.
static void every_adc_fine_but_hospital_low() {
    Setup s;
    assert(s.inventory.addItem(1, "Insulin", 30, 100) == AddItemVerdict::Added);
    assert(s.inventory.addItem(2, "Insulin", 30, 100) == AddItemVerdict::Added);
    assert(s.hospital.addReserve("Insulin", 10, 500) == AddItemVerdict::Added);
    HospitalStockLevel i = s.hospital.check("Insulin");
    assert(i.totalCount == 70 && i.totalCapacity == 700);
    assert(i.verdict == HospitalStockVerdict::Low);
    assert(s.hospital.checkAll() == std::vector<std::string>{"Insulin"});
    assert(s.adcAlarms() == 0 && s.hospitalAlarms() == 1);
    assert(s.notifier.raised[0].type == AlarmType::HospitalLowStock);
    assert(s.notifier.raised[0].detail == "Insulin");
}

// The hospital totals follow the ADC counts as they change, with no extra call.
static void withdrawals_lower_the_hospital_total_until_low() {
    Setup s;
    assert(s.inventory.addItem(1, "Insulin", 60, 100) == AddItemVerdict::Added);
    assert(s.hospital.addReserve("Insulin", 0, 100) == AddItemVerdict::Added);
    assert(s.hospital.check("Insulin").verdict == HospitalStockVerdict::NotLow);  // 60 of 200
    assert(s.inventory.withdraw(1, "Insulin", 10) == InventoryVerdict::Updated);
    assert(s.hospital.check("Insulin").verdict == HospitalStockVerdict::NotLow);  // 50 of 200
    assert(s.inventory.withdraw(1, "Insulin", 1) == InventoryVerdict::Updated);
    assert(s.hospital.check("Insulin").verdict == HospitalStockVerdict::Low);     // 49 of 200
}

// --- checkAll() ---

static void check_all_raises_once_per_low_drug() {
    Setup s;
    assert(s.hospital.addReserve("Insulin", 0, 100) == AddItemVerdict::Added);    // 0 of 100: low
    assert(s.hospital.addReserve("Heparin", 10, 100) == AddItemVerdict::Added);   // 10 of 100: low
    assert(s.hospital.checkAll() == (std::vector<std::string>{"Heparin", "Insulin"}));
    assert(s.notifier.raised.size() == 2);                    // Morphine is not low: no alarm
    for (const auto& a : s.notifier.raised) {
        assert(a.adc == 0 && a.type == AlarmType::HospitalLowStock);
    }
    assert(s.notifier.raised[0].detail == "Heparin" && s.notifier.raised[1].detail == "Insulin");
}

static void check_all_no_alarm_when_not_low() {
    Setup s;
    assert(s.hospital.addReserve("Heparin", 25, 100) == AddItemVerdict::Added);   // exactly 25%
    assert(s.hospital.checkAll().empty());
    assert(s.notifier.raised.empty());
}

// Passes every raised alarm to alarm resolution, as alarm notification will.
class ResolutionNotifier : public AlarmNotifier {
public:
    explicit ResolutionNotifier(AlarmResolution& alarms) : alarms(alarms) {}
    void raise(const AdcId& adc, AlarmType type, const std::string& detail = "") override {
        verdicts.push_back(alarms.add(adc, type, Role::Pharmacist, detail).verdict);
    }
    std::vector<AddVerdict> verdicts;
private:
    AlarmResolution& alarms;
};

// Hospital stock wired to the real alarm resolution. Insulin is held only in
// reserve: 5 of 100, which is low.
struct AlarmSetup {
    FakeClock clock{500};
    FakeHistory history;
    FakePermissions permissions;
    AlarmResolution alarms{clock, history, permissions};
    ResolutionNotifier notifier{alarms};
    InventoryRecord inventory{clock, history, notifier, permissions};
    HospitalStock hospital{clock, inventory, history, notifier, permissions};
    AlarmSetup() {
        assert(hospital.addReserve("Insulin", 5, 100) == AddItemVerdict::Added);
    }
    void setReserve(int count) {
        assert(hospital.manualReserveChange(pharmacist, "Insulin", count, "recount")
               == ReserveVerdict::Updated);
    }
};

static void repeated_check_while_low_raises_nothing_more() {
    AlarmSetup s;
    assert(s.hospital.checkAll() == std::vector<std::string>{"Insulin"});
    s.clock.advance(10);
    assert(s.hospital.checkAll().empty());
    assert(s.notifier.verdicts == std::vector<AddVerdict>{AddVerdict::Added});
    AlarmInfo a = *s.alarms.find(1);
    assert(a.adc == 0 && a.type == AlarmType::HospitalLowStock && a.detail == "Insulin");
    assert(a.severity == Severity::High);
    assert(!s.alarms.find(2).has_value());                     // still one alarm
}

static void no_repeat_after_resolving_while_still_low() {
    AlarmSetup s;
    s.hospital.checkAll();
    assert(s.alarms.resolve(pharmacist, 1, "delivery ordered") == ResolveVerdict::Resolved);
    s.clock.advance(10);
    assert(s.hospital.checkAll().empty());                     // still low, but not again
    s.setReserve(10);                                          // still low (10%)
    assert(s.hospital.checkAll().empty());
    assert(s.notifier.verdicts.size() == 1);
    assert(!s.alarms.find(2).has_value());
}

static void raised_again_after_recovering_and_then_dropping() {
    AlarmSetup s;
    s.hospital.checkAll();
    assert(s.alarms.resolve(pharmacist, 1, "delivery arrived") == ResolveVerdict::Resolved);
    s.setReserve(80);                                          // recovered
    assert(s.hospital.checkAll().empty());
    s.setReserve(10);                                          // low again
    assert(s.hospital.checkAll() == std::vector<std::string>{"Insulin"});
    assert((s.notifier.verdicts == std::vector<AddVerdict>{AddVerdict::Added, AddVerdict::Added}));
    assert(s.alarms.find(2)->state == AlarmState::Raised);     // a new alarm
}

// Recovery is only seen by a check: dropping again before any check counts as
// staying low.
static void recovery_must_be_seen_by_a_check() {
    AlarmSetup s;
    s.hospital.checkAll();
    s.setReserve(80);
    s.setReserve(10);
    assert(s.hospital.checkAll().empty());
}

// --- manualReserveChange() function table ---

static void reserve_row1_technician_not_authorized() {
    Setup s;
    assert(s.hospital.manualReserveChange(technician, "Morphine", 50, "recount")
           == ReserveVerdict::NotAuthorized);
    assert(s.reserve("Morphine") == 300 && s.history.entries.empty());
}

static void reserve_row1_follows_the_permission_table() {
    Setup s;
    s.permissions.set(Role::Pharmacist, Action::ManualInventoryChange, false);
    assert(s.hospital.manualReserveChange(pharmacist, "Morphine", 50, "recount")
           == ReserveVerdict::NotAuthorized);
    s.permissions.set(Role::Technician, Action::ManualInventoryChange, true);
    assert(s.hospital.manualReserveChange(technician, "Morphine", 50, "recount")
           == ReserveVerdict::Updated);
}

static void reserve_row2_unknown_medication() {
    Setup s;
    assert(s.inventory.addItem(1, "Insulin", 50, 100) == AddItemVerdict::Added);  // in an ADC only
    assert(s.hospital.manualReserveChange(pharmacist, "Insulin", 50, "recount")
           == ReserveVerdict::UnknownMedication);
    assert(s.hospital.manualReserveChange(pharmacist, "Paracetamol", 50, "recount")
           == ReserveVerdict::UnknownMedication);
    assert(s.history.entries.empty());
}

static void reserve_row3_negative_count_rejected() {
    Setup s;
    assert(s.hospital.manualReserveChange(pharmacist, "Morphine", -1, "recount")
           == ReserveVerdict::IncorrectValue);
    assert(s.reserve("Morphine") == 300 && s.history.entries.empty());
}

static void reserve_row4_no_explanation_rejected() {
    Setup s;
    assert(s.hospital.manualReserveChange(pharmacist, "Morphine", 50, "")
           == ReserveVerdict::NoExplanation);
    assert(s.hospital.manualReserveChange(pharmacist, "Morphine", 50, "   ")
           == ReserveVerdict::NoExplanation);
    assert(s.reserve("Morphine") == 300 && s.history.entries.empty());
}

static void reserve_row5_history_failure_changes_nothing() {
    Setup s;
    s.history.failWrites(true);
    assert(s.hospital.manualReserveChange(pharmacist, "Morphine", 50, "recount")
           == ReserveVerdict::StorageFailure);
    assert(s.reserve("Morphine") == 300);
}

static void reserve_row6_pharmacist_updates_with_explanation() {
    Setup s;
    s.clock.set(900);
    assert(s.hospital.manualReserveChange(pharmacist, "Morphine", 250, "expired stock removed")
           == ReserveVerdict::Updated);
    assert(s.reserve("Morphine") == 250);
    assert(s.history.entries.size() == 1);
    const HistoryEntry& e = s.history.entries[0];
    assert(e.kind == "ReserveChange" && e.who == "Priya" && e.time == 900);
    assert(e.adc == 0 && e.medication == "Morphine");
    assert(e.detail == "reserve Morphine: 300 -> 250: expired stock removed");
}

static void reserve_row6_administrator_may_update() {
    Setup s;
    assert(s.hospital.manualReserveChange(administrator, "Morphine", 310, "recount")
           == ReserveVerdict::Updated);
    assert(s.reserve("Morphine") == 310);
}

static void reserve_row6_zero_allowed() {
    Setup s;
    assert(s.hospital.manualReserveChange(pharmacist, "Morphine", 0, "reserve emptied")
           == ReserveVerdict::Updated);
    assert(s.reserve("Morphine") == 0);
}

static void reserve_change_raises_no_alarm_until_check_all() {
    Setup s;
    assert(s.hospital.manualReserveChange(pharmacist, "Morphine", 0, "reserve emptied")
           == ReserveVerdict::Updated);                        // 100 of 600: low
    assert(s.notifier.raised.empty());
    assert(s.hospital.checkAll() == std::vector<std::string>{"Morphine"});
    assert(s.hospitalAlarms() == 1);
}

// --- all() ---

static void all_lists_every_medication_in_name_order() {
    Setup s;
    assert(s.hospital.addReserve("Heparin", 150, 200) == AddItemVerdict::Added);  // reserves only
    assert(s.inventory.addItem(2, "Insulin", 5, 100) == AddItemVerdict::Added);   // ADC only
    std::vector<HospitalStockLevel> levels = s.hospital.all();
    assert(levels.size() == 3);
    assert(levels[0].medication == "Heparin" && levels[0].verdict == HospitalStockVerdict::NotLow);
    assert(levels[1].medication == "Insulin" && levels[1].verdict == HospitalStockVerdict::Low);
    assert(levels[2].medication == "Morphine" && levels[2].totalCount == 400);
}

int main() {
    add_reserve_bad_values_rejected();
    add_reserve_twice_changes_nothing();
    totals_are_reserve_plus_every_adc();
    other_medications_are_not_counted();
    medication_with_no_reserve_uses_adcs_only();
    medication_only_in_reserves_is_counted();
    check_row1_unknown_medication();
    check_row2_below_25_percent_of_hospital_capacity_is_low();
    check_row3_exactly_25_percent_is_not_low();
    check_row3_well_stocked_is_not_low();
    one_adc_low_but_hospital_not_low();
    every_adc_fine_but_hospital_low();
    withdrawals_lower_the_hospital_total_until_low();
    check_all_raises_once_per_low_drug();
    check_all_no_alarm_when_not_low();
    repeated_check_while_low_raises_nothing_more();
    no_repeat_after_resolving_while_still_low();
    raised_again_after_recovering_and_then_dropping();
    recovery_must_be_seen_by_a_check();
    reserve_row1_technician_not_authorized();
    reserve_row1_follows_the_permission_table();
    reserve_row2_unknown_medication();
    reserve_row3_negative_count_rejected();
    reserve_row4_no_explanation_rejected();
    reserve_row5_history_failure_changes_nothing();
    reserve_row6_pharmacist_updates_with_explanation();
    reserve_row6_administrator_may_update();
    reserve_row6_zero_allowed();
    reserve_change_raises_no_alarm_until_check_all();
    all_lists_every_medication_in_name_order();
    std::printf("hospital_stock_test: 30 tests passed\n");
    return 0;
}
