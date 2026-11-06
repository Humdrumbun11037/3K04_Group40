//integrated: message-path adapters

#include "gui/integrated/adapters/message_adapters.hpp"

#include "gui/names.hpp"

// The router has already worked out the new count (and raised its own
// DISCREPANCY alarm if the ADC took more than the HMS had). Andrew's inventory
// record applies the change through its own rules: an item it doesn't track is
// refused and reported here, because Inodh's interface has no way to say no.
void InventoryLink::setCount(int adcId, const std::string& medication, int newCount) {
    hms::behaviour::AdcId adc = convert::fromSwAdc(adcId);
    std::optional<int> current = record.count(adc, medication);
    std::string where = hms::behaviour::adcName(adc);
    if (!current) {
        feed.push_back({clock.now(), "message", "Inventory record refused " + medication + " in " + where
                        + ": not stocked there (UnknownItem). The count was not changed."});
        return;
    }
    int difference = newCount - *current;
    if (difference == 0) return;
    InventoryVerdict v = difference > 0 ? record.deposit(adc, medication, difference)
                                        : record.withdraw(adc, medication, -difference);
    feed.push_back({clock.now(), "message", std::string(difference > 0 ? "Deposit " : "Withdrawal ")
                    + std::to_string(difference > 0 ? difference : -difference) + " " + medication + " in " + where
                    + " applied by inventory record: " + nameOf(v, names::INVENTORY)});
}
