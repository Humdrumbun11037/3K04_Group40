//integrated: Matthew's encrypted storage as Andrew's history

#include "gui/integrated/adapters/file_history.hpp"

#include <exception>

namespace hb = hms::behaviour;

// The event type that carries an entry: its own name if Inodh's list has it
// (e.g. "Withdrawal", "UserLogin", "AdcRegistry", "Maintenance"), else the
// closest category. The real kind is always in fields["kind"].
static EventType carrierFor(const std::string& kind) {
    EventType type;
    if (eventTypeFromName(kind, type)) return type;
    if (kind.rfind("Inventory", 0) == 0 || kind.rfind("Reserve", 0) == 0) return EventType::ManualInventoryChange;
    if (kind.rfind("Alarm", 0) == 0) return EventType::Alarm;
    return EventType::Maintenance;   // AdcHealth, ReportFailed, SummaryRequested, ...
}

static std::string field(const Event& event, const char* name) {
    auto found = event.fields.find(name);
    return found == event.fields.end() ? "" : found->second;
}

hb::HistoryResult FileHistory::record(const hb::HistoryEntry& entry) {
    if (writesFail) return hb::HistoryResult::StorageFailure;
    Event event;
    event.type = carrierFor(entry.kind);
    event.adcId = convert::toSwAdc(entry.adc);
    event.timestamp = convert::toText(entry.time);
    event.fields["kind"] = entry.kind;
    event.fields["who"] = entry.who;
    event.fields["detail"] = entry.detail;
    if (!entry.medication.empty()) event.fields["medication"] = entry.medication;
    try {
        trail.append(event);
        return hb::HistoryResult::Stored;
    } catch (const std::exception& e) {
        realFailures++;
        lastError = e.what();
        return hb::HistoryResult::StorageFailure;
    }
}

hb::HistoryReadResult FileHistory::read(long long from, long long to) const {
    if (readsFail) return {hb::ReadVerdict::StorageFailure, {}};
    if (from >= to) return {hb::ReadVerdict::Ok, {}};
    try {
        std::vector<hb::HistoryEntry> entries;
        for (const Event& event : trail.search(convert::toText(from), convert::toText(to - 1))) {
            hb::HistoryEntry entry;
            entry.kind = field(event, "kind").empty() ? eventTypeName(event.type) : field(event, "kind");
            entry.who = field(event, "who");
            entry.detail = field(event, "detail");
            long long seconds = 0;
            entry.time = convert::fromText(event.timestamp, seconds) ? seconds : 0;
            entry.adc = convert::fromSwAdc(event.adcId);
            entry.medication = field(event, "medication");
            entries.push_back(entry);
        }
        return {hb::ReadVerdict::Ok, entries};
    } catch (const std::exception& e) {
        realFailures++;
        lastError = e.what();
        return {hb::ReadVerdict::StorageFailure, {}};
    }
}
