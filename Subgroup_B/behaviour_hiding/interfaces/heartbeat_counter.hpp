//heartbeat counter (interface only)

/*
Owned by the ADC comms link or history (to be agreed). This file only states
what the daily report needs: how many heartbeats an ADC sent in a time window.

Interface
takes: an ADC id and a time window [from, to)
sends: the number of heartbeats received, or a failure
*/
#pragma once

#include "behaviour_hiding/interfaces/adc_id.hpp"

namespace hms::behaviour {

enum class CountVerdict { Ok, Unavailable };

struct HeartbeatCountResult {
    CountVerdict verdict;
    int count;   // only meaningful when verdict == Ok
};

class HeartbeatCounter {
public:
    virtual ~HeartbeatCounter() = default;

    // { Pre: none }
    //   count(adc, from, to)
    // { Post: Ok          => count = heartbeats received from adc with from <= time < to
    //         Unavailable => the count could not be read }
    virtual HeartbeatCountResult count(const AdcId& adc, long long from, long long to) const = 0;
};

}
