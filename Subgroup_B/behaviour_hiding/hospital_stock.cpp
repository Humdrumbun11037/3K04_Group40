//hospital stock

#include "behaviour_hiding/hospital_stock.hpp"

#include <cctype>
#include <set>

using namespace hms::behaviour;

// An explanation that is empty or only whitespace is not an explanation.
static bool isBlank(const std::string& text) {
    for (char c : text) {
        if (!std::isspace(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

HospitalStock::HospitalStock(const Clock& clock, const InventoryRecord& adcs, History& history,
                             AlarmNotifier& notifier, const Permissions& permissions)
    : clock(clock), adcs(adcs), history(history), notifier(notifier), permissions(permissions) {}

AddItemVerdict HospitalStock::addReserve(const std::string& medication, int count, int capacity) {
    if (count < 0 || capacity <= 0)                    return AddItemVerdict::IncorrectValue;
    if (reserves.count(medication) > 0)                return AddItemVerdict::AlreadyExists;
    reserves[medication] = {count, capacity};
    return AddItemVerdict::Added;
}

// Rows are checked in table order, so the first matching row wins.
ReserveVerdict HospitalStock::manualReserveChange(const User& user, const std::string& medication,
                                                  int newCount, const std::string& explanation) {
    auto found = reserves.find(medication);
    if (!permissions.mayPerform(user.role, Action::ManualInventoryChange))
                                                       return ReserveVerdict::NotAuthorized;     // row 1
    if (found == reserves.end())                       return ReserveVerdict::UnknownMedication; // row 2
    Reserve& reserve = found->second;
    if (newCount < 0)                                  return ReserveVerdict::IncorrectValue;    // row 3
    if (isBlank(explanation))                          return ReserveVerdict::NoExplanation;     // row 4
    std::string detail = "reserve " + medication + ": " + std::to_string(reserve.count) + " -> " +
                         std::to_string(newCount) + ": " + explanation;
    if (history.record({"ReserveChange", user.name, detail, clock.now(), HOSPITAL, medication})
        != HistoryResult::Stored)                      return ReserveVerdict::StorageFailure;    // row 5
    reserve.count = newCount;                                                                    // row 6
    return ReserveVerdict::Updated;
}

// Rows are checked in table order, so the first matching row wins.
HospitalStockLevel HospitalStock::check(const std::string& medication) const {
    HospitalStockLevel level{medication, 0, 0, 0, 0, 0, 0, HospitalStockVerdict::NotLow};
    auto reserve = reserves.find(medication);
    if (reserve != reserves.end()) {
        level.reserveCount = reserve->second.count;
        level.reserveCapacity = reserve->second.capacity;
    }
    for (const StockItem& item : adcs.items()) {
        if (item.medication != medication) continue;
        level.adcCount += item.count;
        level.adcCapacity += item.capacity;
    }
    level.totalCount = level.reserveCount + level.adcCount;
    level.totalCapacity = level.reserveCapacity + level.adcCapacity;

    if (level.totalCapacity == 0)                      level.verdict = HospitalStockVerdict::UnknownMedication; // row 1
    else if (lowStock.check(level.totalCount, level.totalCapacity) == LowStockVerdict::Low)
                                                       level.verdict = HospitalStockVerdict::Low;               // row 2
    else                                               level.verdict = HospitalStockVerdict::NotLow;            // row 3
    return level;
}

std::vector<HospitalStockLevel> HospitalStock::all() const {
    std::set<std::string> medications;
    for (const auto& [medication, reserve] : reserves) medications.insert(medication);
    for (const StockItem& item : adcs.items()) medications.insert(item.medication);

    std::vector<HospitalStockLevel> levels;
    for (const std::string& medication : medications) levels.push_back(check(medication));
    return levels;
}

// Raised only when a medication goes from not low to low, so a medication that
// stays low is alarmed once, and not again after that alarm is resolved, until
// it has recovered and dropped again.
std::vector<std::string> HospitalStock::checkAll() {
    std::vector<std::string> alarmed;
    for (const HospitalStockLevel& level : all()) {
        if (level.verdict != HospitalStockVerdict::Low) {
            lowAtLastCheck.erase(level.medication);
            continue;
        }
        if (!lowAtLastCheck.insert(level.medication).second) continue;   // still low: no repeat
        notifier.raise(HOSPITAL, AlarmType::HospitalLowStock, level.medication);
        alarmed.push_back(level.medication);
    }
    return alarmed;
}
