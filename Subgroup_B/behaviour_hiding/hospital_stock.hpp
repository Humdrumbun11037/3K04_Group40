//hospital stock

/*
Hides: how stock is counted across the whole hospital: one reserve count and
       capacity per medication (the pharmacy stores, held here), added to that
       medication's count and capacity in every ADC, and judged with the same
       low stock rule (25%) as a single ADC (FR-INV-01, FR-INV-02)

Interface
takes: reserve levels per medication, manual reserve changes by users,
       the ADC counts from inventory record
sends: per-medication hospital totals and whether each is low,
       HospitalLowStock alarms (ADC id 0, the medication as detail),
       reserve changes to history
uses: low stock rule, inventory record, clock, history, alarm notifier, permissions

Invariants:
- every reserve count is 0 or more, and every reserve capacity is more than 0
- a reserve count only changes if the change was stored in history
*/
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "behaviour_hiding/inventory_record.hpp"
#include "behaviour_hiding/low_stock_rule.hpp"
#include "behaviour_hiding/interfaces/alarm_notifier.hpp"
#include "behaviour_hiding/interfaces/clock.hpp"
#include "behaviour_hiding/interfaces/history.hpp"
#include "behaviour_hiding/interfaces/permissions.hpp"
#include "behaviour_hiding/interfaces/user.hpp"

enum class HospitalStockVerdict { NotLow, Low, UnknownMedication };
enum class ReserveVerdict { Updated, NotAuthorized, UnknownMedication, IncorrectValue, NoExplanation,
                            StorageFailure };

// One medication's stock across the hospital.
struct HospitalStockLevel {
    std::string medication;
    int reserveCount;
    int reserveCapacity;
    int adcCount;         // summed over every ADC that holds the medication
    int adcCapacity;
    int totalCount;       // reserve + ADCs
    int totalCapacity;
    HospitalStockVerdict verdict;
};

class HospitalStock {
public:
    // The ADC id used for hospital-wide alarms and history entries (shown as "Hospital").
    static constexpr hms::behaviour::AdcId HOSPITAL = 0;

    // { Post: no reserves }
    HospitalStock(const hms::behaviour::Clock& clock, const InventoryRecord& adcs,
                  hms::behaviour::History& history, hms::behaviour::AlarmNotifier& notifier,
                  const hms::behaviour::Permissions& permissions);

    // { Pre: none }
    //   addReserve(medication, count, capacity)
    // { Post: count < 0 or capacity <= 0  => IncorrectValue, nothing added
    //         reserve already tracked     => AlreadyExists, nothing changed
    //         otherwise                   => Added (no history entry, no alarm) }
    AddItemVerdict addReserve(const std::string& medication, int count, int capacity);

    // { Pre: none }
    //   manualReserveChange(user, medication, newCount, explanation)
    // { Post: the first matching row decides the result:
    //         1. permissions deny ManualInventoryChange  => NotAuthorized
    //         2. no reserve for the medication           => UnknownMedication
    //         3. newCount < 0                            => IncorrectValue
    //         4. explanation empty or only whitespace    => NoExplanation
    //         5. history write fails                     => StorageFailure
    //         6. otherwise                               => Updated, reserve count = newCount,
    //                                                       recorded ("ReserveChange") with
    //                                                       the explanation
    //         Rows 1-5 change nothing. No alarm is raised here; checkAll() does that. }
    ReserveVerdict manualReserveChange(const hms::behaviour::User& user, const std::string& medication,
                                       int newCount, const std::string& explanation);

    // { Pre: none }
    //   check(medication)
    // { Post: totals = reserve (if any) + every ADC holding the medication;
    //         the first matching row decides the verdict:
    //         1. no reserve and no ADC holds it             => UnknownMedication, all zero
    //         2. total count strictly below 25% of the
    //            total capacity (low stock rule)            => Low
    //         3. otherwise                                  => NotLow }
    HospitalStockLevel check(const std::string& medication) const;

    // { Pre: none }
    //   all()
    // { Post: check() for every medication in the reserves or any ADC,
    //         ordered by medication name }
    std::vector<HospitalStockLevel> all() const;

    // { Pre: none }
    //   checkAll()
    // { Post: for every medication in all() that is Low now but was not Low at the
    //         previous checkAll() (or has never been checked), a HospitalLowStock
    //         alarm has been raised for ADC id 0 with the medication as its detail.
    //         A medication that stays Low raises nothing more, even after its alarm
    //         is resolved; it must be seen not Low by a checkAll() first.
    //         Returns the medications alarmed by this call, in name order. }
    std::vector<std::string> checkAll();

private:
    struct Reserve {
        int count;
        int capacity;
    };

    const hms::behaviour::Clock& clock;
    const InventoryRecord& adcs;
    hms::behaviour::History& history;
    hms::behaviour::AlarmNotifier& notifier;
    const hms::behaviour::Permissions& permissions;
    LowStockRule lowStock;
    std::map<std::string, Reserve> reserves;
    std::set<std::string> lowAtLastCheck;   // medications that were Low at the last checkAll()
};
