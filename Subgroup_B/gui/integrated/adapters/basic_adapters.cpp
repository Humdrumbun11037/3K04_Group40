//integrated: small adapters

#include "gui/integrated/adapters/basic_adapters.hpp"

// Event -> HistoryEntry:
//   kind   = the event type's name (e.g. "Withdrawal", "UserLogin", "AdcRegistry")
//   who    = the "user" field, else the ADC, else "HMS"
//   detail = every other field as name=value
//   time   = the event's text time in seconds (clock now if it has none)
//   adc    = -1 becomes 0; medication = the "medication" field
void EventToHistory::record(const Event& event) {
    hms::behaviour::HistoryEntry entry;
    entry.kind = eventTypeName(event.type);
    auto user = event.fields.find("user");
    hms::behaviour::AdcId adc = convert::fromSwAdc(event.adcId);
    entry.who = (user != event.fields.end() && !user->second.empty()) ? user->second
              : adc != 0 ? hms::behaviour::adcName(adc) : "HMS";
    for (const auto& [name, value] : event.fields) {
        if (name == "user") continue;
        entry.detail += (entry.detail.empty() ? "" : " ") + name + "=" + value;
    }
    long long seconds = 0;
    entry.time = convert::fromText(event.timestamp, seconds) ? seconds : clock.now();
    entry.adc = adc;
    auto medication = event.fields.find("medication");
    entry.medication = medication != event.fields.end() ? medication->second : "";
    if (history.record(entry) != hms::behaviour::HistoryResult::Stored) failures++;
}
