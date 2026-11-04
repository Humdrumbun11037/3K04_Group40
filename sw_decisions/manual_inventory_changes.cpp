//manual inventory changes

/*
Hides: the process for implementing manual inventory changes

Receives: the user command for performing a manual inventory change from the user dashboard
Sends: updated inventory counts to the inventory record,
       a message to the user if the change was rejected,
       the change to report_generation
*/

#include "manual_inventory_changes.hpp"

#include <map>

ManualInventoryChanges::ManualInventoryChanges(InventoryStore& inventory, const ADClist& adcs,
                                               DashboardOutput& dashboard, EventRecorder& recorder,
                                               const Clock& clock)
    : inventory_(inventory), adcs_(adcs), dashboard_(dashboard), recorder_(recorder), clock_(clock) {}

ManualInventoryChanges::Result ManualInventoryChanges::apply(
        const std::string& userId, int adcId, const std::vector<InventoryChange>& changes) {

    auto reject = [&](const std::string& reason) {
        // If the requested change is invalid, send the user a message packet
        // saying it's not and the reason why
        dashboard_.deliver(userId, "Inventory change on ADC " + std::to_string(adcId) +
                                       " rejected: " + reason);
        return Result{false, reason};
    };

    // Check if the change is valid: the ADC and medications exist and the
    // final number is still a positive number or zero
    if (!adcs_.exists(adcId)) {
        return reject("ADC " + std::to_string(adcId) + " is not registered.");
    }
    if (changes.empty()) {
        return reject("No changes were given.");
    }

    // The same medication may be listed more than once; combine first.
    std::map<std::string, long long> combined;
    for (const InventoryChange& change : changes) {
        if (change.medication.empty()) return reject("A medication name is missing.");
        if (change.delta == 0) return reject("Change for " + change.medication + " is zero.");
        combined[change.medication] += change.delta;
    }

    std::map<std::string, int> finalCounts;
    for (const auto& entry : combined) {
        const std::string& medication = entry.first;
        if (!inventory_.hasMedication(adcId, medication)) {
            return reject(medication + " is not stocked in ADC " + std::to_string(adcId) + ".");
        }
        const long long current = inventory_.count(adcId, medication);
        const long long result = current + entry.second;
        if (result < 0) {
            return reject("Removing " + std::to_string(-entry.second) + " " + medication +
                          " would leave " + std::to_string(result) + " (only " +
                          std::to_string(current) + " in stock).");
        }
        if (result > 1000000) {
            return reject("Resulting count for " + medication + " is unrealistically large.");
        }
        finalCounts[medication] = static_cast<int>(result);
    }

    // Else update the inventory record based on the medications and quantities provided
    const std::string now = clock_.now();
    for (const auto& entry : finalCounts) {
        const int before = inventory_.count(adcId, entry.first);
        inventory_.setCount(adcId, entry.first, entry.second);

        Event event;
        event.type = EventType::ManualInventoryChange;
        event.adcId = adcId;
        event.timestamp = now;
        event.fields["user"] = userId;
        event.fields["medication"] = entry.first;
        event.fields["before"] = std::to_string(before);
        event.fields["after"] = std::to_string(entry.second);
        recorder_.record(event);
    }

    dashboard_.deliver(userId, "Inventory change on ADC " + std::to_string(adcId) + " applied.");
    return Result{true, ""};
}
