//inventory record

/*
Hides: how medication counts are kept (one count and capacity per medication
       per ADC) and which changes are valid (FR-INV-01, FR-INV-03, FR-INV-04)

Interface
takes: items to track, withdrawals and deposits from ADC uploads, manual changes by users
sends: counts, entries to history, low-stock and discrepancy alarms
uses: low stock rule, clock, history, alarm notifier, permissions

Invariants:
- every count is 0 or more, and every capacity is more than 0
- a count only changes if the change was stored in history
*/
#pragma once

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "behaviour_hiding/low_stock_rule.hpp"
#include "behaviour_hiding/interfaces/adc_id.hpp"
#include "behaviour_hiding/interfaces/alarm_notifier.hpp"
#include "behaviour_hiding/interfaces/clock.hpp"
#include "behaviour_hiding/interfaces/history.hpp"
#include "behaviour_hiding/interfaces/permissions.hpp"
#include "behaviour_hiding/interfaces/user.hpp"

enum class AddItemVerdict { Added, AlreadyExists, IncorrectValue };
enum class InventoryVerdict { Updated, NotAuthorized, UnknownItem, IncorrectValue, NoExplanation,
                              StorageFailure, DiscrepancyRecorded };

// A copy of one tracked item, for callers to read.
struct StockItem {
    hms::behaviour::AdcId adc;
    std::string medication;
    int count;
    int capacity;
};

// The HMS's copy of how much of each medication is in each ADC. Every change is
// written to history first, so the counts can always be explained later.
class InventoryRecord {
public:
    // { Post: no items }
    InventoryRecord(const hms::behaviour::Clock& clock, hms::behaviour::History& history,
                    hms::behaviour::AlarmNotifier& notifier,
                    const hms::behaviour::Permissions& permissions);

    // Sets up a medication in an ADC when the system is configured.
    // { Pre: none }
    //   addItem(adc, medication, count, capacity)
    // { Post: adc <= 0, count < 0,
    //         or capacity <= 0             => IncorrectValue, nothing added
    //         item already tracked         => AlreadyExists, nothing changed
    //         otherwise                    => Added (no history entry, no alarm) }
    AddItemVerdict addItem(const hms::behaviour::AdcId& adc, const std::string& medication,
                           int count, int capacity);

    // Applies a withdrawal reported by an ADC. If the ADC took more than the HMS
    // thought it had, the gap is recorded and flagged rather than refused.
    // { Pre: the upload was already validated by message processing }
    //   withdraw(adc, medication, amount)
    // { Post: the first matching row decides the result:
    //         1. item not tracked                     => UnknownItem
    //         2. amount <= 0                          => IncorrectValue
    //         3. history write fails                  => StorageFailure
    //         4. amount > count (the ADC has already
    //            dispensed, so it is recorded)        => DiscrepancyRecorded, count = 0,
    //                                                    recorded in history, and an
    //                                                    InventoryDiscrepancy alarm raised
    //                                                    with the medication as its detail
    //         5. otherwise                            => Updated, count -= amount,
    //                                                    recorded in history
    //         After rows 4 and 5, if the count is low, an InventoryLow alarm is
    //         raised with the medication as its detail.
    //         Rows 1-3 change nothing. }
    InventoryVerdict withdraw(const hms::behaviour::AdcId& adc, const std::string& medication,
                              int amount);

    // Applies a restock reported by an ADC.
    // { Pre: the upload was already validated by message processing }
    //   deposit(adc, medication, amount)
    // { Post: the first matching row decides the result:
    //         1. item not tracked                     => UnknownItem
    //         2. amount <= 0, or the new count would
    //            not fit in an int                    => IncorrectValue
    //         3. history write fails                  => StorageFailure
    //         4. otherwise                            => Updated, count += amount
    //                                                    (above capacity is allowed),
    //                                                    recorded in history
    //         Rows 1-3 change nothing. No alarm is raised. }
    InventoryVerdict deposit(const hms::behaviour::AdcId& adc, const std::string& medication,
                             int amount);

    // A pharmacist or administrator corrects a count by hand, for example after a
    // stock take. An explanation is required so the change can be audited.
    // { Pre: none }
    //   manualChange(user, adc, medication, newCount, explanation)
    // { Post: the first matching row decides the result:
    //         1. permissions deny ManualInventoryChange  => NotAuthorized
    //         2. item not tracked                        => UnknownItem
    //         3. newCount < 0                            => IncorrectValue
    //         4. explanation empty or only whitespace    => NoExplanation
    //         5. history write fails                     => StorageFailure
    //         6. otherwise                               => Updated, count = newCount,
    //                                                       recorded with the explanation
    //         After row 6, if the count is low, an InventoryLow alarm is raised
    //         with the medication as its detail.
    //         Rows 1-5 change nothing. }
    InventoryVerdict manualChange(const hms::behaviour::User& user, const hms::behaviour::AdcId& adc,
                                  const std::string& medication, int newCount,
                                  const std::string& explanation);

    // Reads one count (nothing if the medication is not stocked in that ADC).
    // { Pre: none }
    //   count(adc, medication)
    // { Post: the current count, or nothing if the item is not tracked }
    std::optional<int> count(const hms::behaviour::AdcId& adc, const std::string& medication) const;

    // Lists everything tracked, for the stock screen and for hospital stock.
    // { Pre: none }
    //   items()
    // { Post: a copy of every tracked item, ordered by ADC then medication }
    std::vector<StockItem> items() const;

private:
    struct Stock {
        int count;
        int capacity;
    };
    using Key = std::pair<hms::behaviour::AdcId, std::string>;   // (ADC, medication)

    bool store(const std::string& kind, const std::string& who, const std::string& detail,
               const Key& key);
    void raiseIfLow(const Key& key, const Stock& stock);

    const hms::behaviour::Clock& clock;
    hms::behaviour::History& history;
    hms::behaviour::AlarmNotifier& notifier;
    const hms::behaviour::Permissions& permissions;
    LowStockRule lowStock;
    std::map<Key, Stock> stock;
};
