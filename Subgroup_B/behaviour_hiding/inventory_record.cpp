//inventory record

#include "behaviour_hiding/inventory_record.hpp"

#include <cctype>
#include <climits>

using namespace hms::behaviour;

// An explanation that is empty or only whitespace is not an explanation.
static bool isBlank(const std::string& text) {
    for (char c : text) {
        if (!std::isspace(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

// Builds the "Morphine: 40 -> 30" part of a history entry.
static std::string change(const std::string& medication, int before, int after) {
    return medication + ": " + std::to_string(before) + " -> " + std::to_string(after);
}

InventoryRecord::InventoryRecord(const Clock& clock, History& history, AlarmNotifier& notifier,
                                 const Permissions& permissions)
    : clock(clock), history(history), notifier(notifier), permissions(permissions) {}

// Set-up only: no history entry and no alarm, because nothing has happened yet.
AddItemVerdict InventoryRecord::addItem(const AdcId& adc, const std::string& medication,
                                        int count, int capacity) {
    if (adc <= 0 || count < 0 || capacity <= 0)       return AddItemVerdict::IncorrectValue;
    Key key{adc, medication};
    if (stock.count(key) > 0)                          return AddItemVerdict::AlreadyExists;
    stock[key] = {count, capacity};
    return AddItemVerdict::Added;
}

// Rows are checked in table order, so the first matching row wins.
InventoryVerdict InventoryRecord::withdraw(const AdcId& adc, const std::string& medication,
                                           int amount) {
    Key key{adc, medication};
    auto found = stock.find(key);
    if (found == stock.end())                          return InventoryVerdict::UnknownItem;    // row 1
    Stock& item = found->second;
    if (amount <= 0)                                   return InventoryVerdict::IncorrectValue; // row 2
    bool discrepancy = amount > item.count;
    int after = discrepancy ? 0 : item.count - amount;
    std::string detail = "withdrew " + std::to_string(amount) + " " + change(medication, item.count, after);
    if (discrepancy) detail += " (discrepancy: HMS count was " + std::to_string(item.count) + ")";
    if (!store(discrepancy ? "InventoryDiscrepancy" : "InventoryWithdrawal", adcName(adc), detail, key))
                                                       return InventoryVerdict::StorageFailure; // row 3
    item.count = after;
    if (discrepancy) {                                                                          // row 4
        notifier.raise(adc, AlarmType::InventoryDiscrepancy, medication);
        raiseIfLow(key, item);
        return InventoryVerdict::DiscrepancyRecorded;
    }
    raiseIfLow(key, item);                                                                      // row 5
    return InventoryVerdict::Updated;
}

// Rows are checked in table order, so the first matching row wins.
InventoryVerdict InventoryRecord::deposit(const AdcId& adc, const std::string& medication,
                                          int amount) {
    Key key{adc, medication};
    auto found = stock.find(key);
    if (found == stock.end())                          return InventoryVerdict::UnknownItem;    // row 1
    Stock& item = found->second;
    if (amount <= 0 || amount > INT_MAX - item.count)  return InventoryVerdict::IncorrectValue; // row 2
    int after = item.count + amount;
    if (!store("InventoryDeposit", adcName(adc), "deposited " + std::to_string(amount) + " " +
               change(medication, item.count, after), key))
                                                       return InventoryVerdict::StorageFailure; // row 3
    item.count = after;                                                                         // row 4
    return InventoryVerdict::Updated;
}

// Rows are checked in table order, so the first matching row wins.
InventoryVerdict InventoryRecord::manualChange(const User& user, const AdcId& adc,
                                               const std::string& medication, int newCount,
                                               const std::string& explanation) {
    Key key{adc, medication};
    auto found = stock.find(key);
    if (!permissions.mayPerform(user.role, Action::ManualInventoryChange))
                                                       return InventoryVerdict::NotAuthorized;  // row 1
    if (found == stock.end())                          return InventoryVerdict::UnknownItem;    // row 2
    Stock& item = found->second;
    if (newCount < 0)                                  return InventoryVerdict::IncorrectValue; // row 3
    if (isBlank(explanation))                          return InventoryVerdict::NoExplanation;  // row 4
    if (!store("InventoryManualChange", user.name,
               change(medication, item.count, newCount) + ": " + explanation, key))
                                                       return InventoryVerdict::StorageFailure; // row 5
    item.count = newCount;                                                                      // row 6
    raiseIfLow(key, item);
    return InventoryVerdict::Updated;
}

std::optional<int> InventoryRecord::count(const AdcId& adc, const std::string& medication) const {
    auto found = stock.find({adc, medication});
    if (found == stock.end()) return std::nullopt;
    return found->second.count;
}

std::vector<StockItem> InventoryRecord::items() const {
    std::vector<StockItem> all;
    for (const auto& [key, item] : stock) {
        all.push_back({key.first, key.second, item.count, item.capacity});
    }
    return all;
}

// Writes one inventory change to history and says whether it was saved; a count
// only changes if it was.
bool InventoryRecord::store(const std::string& kind, const std::string& who,
                            const std::string& detail, const Key& key) {
    return history.record({kind, who, detail, clock.now(), key.first, key.second})
           == HistoryResult::Stored;
}

// Raised after every change that leaves the count low, with the medication as
// the detail; repeats for the same ADC and medication join the open alarm (FR-ALM-04).
void InventoryRecord::raiseIfLow(const Key& key, const Stock& item) {
    if (lowStock.check(item.count, item.capacity) == LowStockVerdict::Low) {
        notifier.raise(key.first, AlarmType::InventoryLow, key.second);
    }
}
