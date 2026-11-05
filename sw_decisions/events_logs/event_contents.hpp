//event contents

/*
Hides: what constitutes an event

Receives: a stripped and processed message from message_routing,
          the list of rules from event_formatting
Sends: the list of events to report_generation
*/
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "../external_interfaces.hpp"
#include "../report_generation.hpp"
#include "event_formatting.h"

class EventScanner {
public:
    EventScanner(const Clock& clock, EventRecorder& recorder);

    // tokens: the message from one ADC with the ADC id already removed,
    //         e.g. {"Withdrawal", "Morphine", "5", "U1001"}
    // If the tokens contain an event, it is timestamped, sent to report
    // generation, and returned. Otherwise nothing is sent and nullopt is
    // returned.
    std::optional<Event> scan(int adcId, const std::vector<std::string>& tokens);

private:
    const Clock& clock_;
    EventRecorder& recorder_;
};
