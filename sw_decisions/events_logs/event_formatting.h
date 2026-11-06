//event formatting

/*
Hides: the rules list used in eventScanner in event_contents
       (which keywords start an event, and what fields follow each keyword)

Receives: N/A
Sends: the rules list to event_contents

ADC message format (after decryption and patient information stripping):
    <adcId>|<keyword>|<field 1>|<field 2>|...

    e.g. "12|Withdrawal|Morphine|5|U1001"
         "12|Alarm|TEMPERATURE|Fridge above 8C"
*/
#pragma once

#include <map>
#include <string>
#include <vector>

enum class EventType {
    Alarm,
    Withdrawal,
    Deposit,
    Login,
    // Events created inside the HMS itself, not parsed from ADC messages
    ManualInventoryChange,
    AlarmResolved,
    AlarmEscalated,
    Maintenance,
    AdcRegistry,
    UserLogin
};

// One event, ready to be logged by report_generation.
struct Event {
    EventType type;
    int adcId = -1;                              // -1 if not tied to an ADC
    std::string timestamp;                       // "YYYY-MM-DD HH:MM:SS"
    std::map<std::string, std::string> fields;   // named fields from the rule
};

// One rule: a keyword and the names of the fields that follow it.
struct EventRule {
    std::string keyword;
    EventType type;
    std::vector<std::string> fieldNames;
};

inline const std::vector<EventRule>& eventRules() {
    static const std::vector<EventRule> rules = {
        {"Alarm",      EventType::Alarm,      {"code", "description"}},
        {"Withdrawal", EventType::Withdrawal, {"medication", "quantity", "user"}},
        {"Deposit",    EventType::Deposit,    {"medication", "quantity", "user"}},
        {"Login",      EventType::Login,      {"user", "result"}},
    };
    return rules;
}

// Returns nullptr if the keyword is not the start of an event.
inline const EventRule* findRule(const std::string& keyword) {
    for (const auto& rule : eventRules()) {
        if (rule.keyword == keyword) return &rule;
    }
    return nullptr;
}

// Name of each event type, used for storage and for reports.
inline std::string eventTypeName(EventType type) {
    switch (type) {
        case EventType::Alarm:                 return "Alarm";
        case EventType::Withdrawal:            return "Withdrawal";
        case EventType::Deposit:               return "Deposit";
        case EventType::Login:                 return "Login";
        case EventType::ManualInventoryChange: return "ManualInventoryChange";
        case EventType::AlarmResolved:         return "AlarmResolved";
        case EventType::AlarmEscalated:        return "AlarmEscalated";
        case EventType::Maintenance:           return "Maintenance";
        case EventType::AdcRegistry:           return "AdcRegistry";
        case EventType::UserLogin:             return "UserLogin";
    }
    return "Unknown";
}

// Inverse of eventTypeName. Returns false if the name is unknown.
inline bool eventTypeFromName(const std::string& name, EventType& out) {
    static const EventType all[] = {
        EventType::Alarm, EventType::Withdrawal, EventType::Deposit,
        EventType::Login, EventType::ManualInventoryChange,
        EventType::AlarmResolved, EventType::AlarmEscalated,
        EventType::Maintenance, EventType::AdcRegistry, EventType::UserLogin};
    for (EventType t : all) {
        if (eventTypeName(t) == name) { out = t; return true; }
    }
    return false;
}
