//inventory record

#include "behaviour_hiding/inventory_record.hpp"

#include <cctype>

using namespace hms::behaviour;

// Who may change a count by hand.
static bool mayChangeInventory(Role role) {
    switch (role) {
        case Role::Pharmacist:    return true;
        case Role::Administrator: return true;
        case Role::Technician:    return false;
    }
    return false;   // a role not listed above
}

// An explanation that is empty or only whitespace is not an explanation.
static bool isBlank(const std::string& text) {
    for (char c : text) {
        if (!std::isspace(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

static std::string change(const std::string& medication, int before, int after) {
    return medication + ": " + std::to_string(before) + " -> " + std::to_string(after);
}

InventoryRecord::InventoryRecord(const Clock& clock, History& history, AlarmNotifier& notifier)
    : clock(clock), history(history), notifier(notifier) {}

AddItemVerdict InventoryRecord::addItem(const AdcId& adc, const std::string& medication,
                                        int count, int capacity) {
    if (count < 0 || capacity <= 0)                   return AddItemVerdict::IncorrectValue;
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
    if (amount < 0 || amount > item.count)             return InventoryVerdict::IncorrectValue; // row 2
    int after = item.count - amount;
    if (!store("InventoryWithdrawal", adc, "withdrew " + std::to_string(amount) + " " +
               change(medication, item.count, after), key))
                                                       return InventoryVerdict::StorageFailure; // row 3
    item.count = after;                                                                         // row 4
    raiseIfLow(key, item);
    return InventoryVerdict::Updated;
}

// Rows are checked in table order, so the first matching row wins.
InventoryVerdict InventoryRecord::manualChange(const User& user, const AdcId& adc,
                                               const std::string& medication, int newCount,
                                               const std::string& explanation) {
    Key key{adc, medication};
    auto found = stock.find(key);
    if (!mayChangeInventory(user.role))                return InventoryVerdict::NotAuthorized;  // row 1
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
