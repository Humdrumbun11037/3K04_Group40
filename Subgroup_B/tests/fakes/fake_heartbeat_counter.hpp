//fake heartbeat counter

/*
The test (or demo) records heartbeat times with record(adc, time); count()
counts them in a window. failCounts(true) makes every count() Unavailable.
*/
#pragma once

#include <map>
#include <vector>

#include "behaviour_hiding/interfaces/heartbeat_counter.hpp"

class FakeHeartbeatCounter : public hms::behaviour::HeartbeatCounter {
public:
    hms::behaviour::HeartbeatCountResult count(const hms::behaviour::AdcId& adc,
                                               long long from, long long to) const override {
        if (countsFail) return {hms::behaviour::CountVerdict::Unavailable, 0};
        int n = 0;
        auto found = times.find(adc);
        if (found != times.end()) {
            for (long long t : found->second) {
                if (from <= t && t < to) n++;
            }
        }
        return {hms::behaviour::CountVerdict::Ok, n};
    }

    void record(const hms::behaviour::AdcId& adc, long long time) { times[adc].push_back(time); }
    void failCounts(bool fail) { countsFail = fail; }

private:
    std::map<hms::behaviour::AdcId, std::vector<long long>> times;
    bool countsFail = false;
};
