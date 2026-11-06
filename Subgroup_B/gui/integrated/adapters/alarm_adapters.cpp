//integrated: alarm adapters

#include "gui/integrated/adapters/alarm_adapters.hpp"

#include "gui/names.hpp"

namespace hb = hms::behaviour;

namespace alarm_codes {

struct Code { AlarmType type; const char* code; };
static const Code TABLE[] = {
    {AlarmType::UnauthorizedAccess,     "UNAUTHORIZED_ACCESS"},
    {AlarmType::AdcOffline,             "ADC_OFFLINE"},
    {AlarmType::IncorrectMessageFormat, "INCORRECT_MESSAGE_FORMAT"},
    {AlarmType::InventoryLow,           "INVENTORY_LOW"},
    {AlarmType::ConnectionLoss,         "CONNECTION_LOSS"},
    {AlarmType::HospitalLowStock,       "HOSPITAL_LOW_STOCK"},
    {AlarmType::InventoryDiscrepancy,   "DISCREPANCY"},
};

std::string codeOf(AlarmType type) {
    for (const Code& c : TABLE) {
        if (c.type == type) return c.code;
    }
    return "UNKNOWN";
}

bool typeOf(const std::string& code, AlarmType& type) {
    for (const Code& c : TABLE) {
        if (code == c.code) { type = c.type; return true; }
    }
    return false;
}

}  // namespace alarm_codes

SwAlarmSeverity SeverityRules::classify(const std::string& alarmCode) const {
    AlarmType type;
    if (!alarm_codes::typeOf(alarmCode, type)) return SwAlarmSeverity::Critical;
    switch (table.severityOf(type).severity) {
        case Severity::Low:      return SwAlarmSeverity::Low;
        case Severity::High:     return SwAlarmSeverity::High;
        case Severity::Critical: return SwAlarmSeverity::Critical;
    }
    return SwAlarmSeverity::Critical;
}

void AlarmLinks::sync(const ::AlarmNotifier& notifier) {
    for (const AlarmRecord& record : notifier.pending()) {
        if (links.count(record.id) > 0 || unmatched.count(record.id) > 0) continue;
        AlarmType type;
        if (!alarm_codes::typeOf(record.code, type)) {
            unmatched.insert(record.id);
            feed.push_back({clock.now(), "alarm", "ADC alarm code " + record.code + " (" + record.description
                            + ") has no alarm type in alarm resolution: delivered by notification only"});
            continue;
        }
        // Alarm resolution keeps one destination: the first role in Inodh's chart.
        hb::Role destination = convert::toRole(::AlarmNotifier::recipientsFor(record.severity).front());
        hb::AdcId adc = convert::fromSwAdc(record.adcId);
        AddResult r = alarms.add(adc, type, destination, record.description);
        links[record.id] = r.id;
        feed.push_back({clock.now(), "alarm", std::string(nameOf(type, names::ALARM_TYPE))
                        + (record.description.empty() ? "" : " (" + record.description + ")") + " for "
                        + hb::adcName(adc) + ": notification #" + std::to_string(record.id) + " delivered; "
                        + nameOf(r.verdict, names::ADD) + " alarm #" + std::to_string(r.id) + " ("
                        + ::AlarmNotifier::severityName(record.severity) + ")"});
    }
}

bool AlarmLinks::isResolved(int swId) const {
    std::optional<AlarmId> id = mine(swId);
    if (!id) return false;
    std::optional<AlarmInfo> info = alarms.find(*id);
    return info && info->state == AlarmState::Resolved;
}

std::optional<AlarmId> AlarmLinks::mine(int swId) const {
    auto found = links.find(swId);
    if (found == links.end()) return std::nullopt;
    return found->second;
}
