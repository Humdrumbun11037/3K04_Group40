//event contents

/*
Hides: what constitutes an event

Receives: a stripped and processed message from message_routing,
          the list of rules from event_formatting
Sends: the list of events to report_generation
*/

#include "event_contents.hpp"

#include <cctype>

namespace {

bool isNonNegativeInteger(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

// Extra checks on top of "the right number of fields is present".
bool fieldsAreValid(const Event& event) {
    switch (event.type) {
        case EventType::Withdrawal:
        case EventType::Deposit:
            return !event.fields.at("medication").empty() &&
                   isNonNegativeInteger(event.fields.at("quantity"));
        case EventType::Alarm:
            return !event.fields.at("code").empty();
        case EventType::Login:
            return !event.fields.at("user").empty();
        default:
            return true;
    }
}

} // namespace

EventScanner::EventScanner(const Clock& clock, EventRecorder& recorder)
    : clock_(clock), recorder_(recorder) {}

std::optional<Event> EventScanner::scan(int adcId, const std::vector<std::string>& tokens) {
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        const EventRule* rule = findRule(tokens[i]);
        if (rule == nullptr) continue;

        // Parse the next segment of the message based on the rules for this type
        if (tokens.size() - (i + 1) < rule->fieldNames.size()) {
            return std::nullopt;   // message cut short; not a valid event
        }

        Event event;
        event.type = rule->type;
        event.adcId = adcId;
        event.timestamp = clock_.now();
        for (std::size_t f = 0; f < rule->fieldNames.size(); ++f) {
            event.fields[rule->fieldNames[f]] = tokens[i + 1 + f];
        }

        if (!fieldsAreValid(event)) return std::nullopt;

        // Send the event to the report generator with a timestamp
        recorder_.record(event);
        return event;
    }
    return std::nullopt;
}
