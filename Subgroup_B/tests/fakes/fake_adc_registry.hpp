//fake adc registry

/*
An ADC registry the test controls: add ADCs, then decommission them.
*/
#pragma once

#include <set>
#include <vector>

#include "behaviour_hiding/interfaces/adc_registry.hpp"

class FakeAdcRegistry : public hms::behaviour::AdcRegistry {
public:
    bool isActive(const hms::behaviour::AdcId& adc) const override {
        return active.count(adc) > 0;
    }

    std::vector<hms::behaviour::AdcId> activeAdcs() const override {
        return {active.begin(), active.end()};
    }

    void add(const hms::behaviour::AdcId& adc) { active.insert(adc); }
    void decommission(const hms::behaviour::AdcId& adc) { active.erase(adc); }

private:
    std::set<hms::behaviour::AdcId> active;
};
