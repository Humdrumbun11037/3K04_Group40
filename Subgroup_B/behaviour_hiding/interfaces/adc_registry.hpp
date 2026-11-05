//adc registry (interface only)

/*
Owned by the ADC registry (software decisions). This file only states what the
behaviour modules need from it.

Interface
takes: an ADC id
sends: whether that ADC is registered and not decommissioned; the list of such ADCs
*/
#pragma once

#include <vector>

#include "behaviour_hiding/interfaces/adc_id.hpp"

namespace hms::behaviour {

class AdcRegistry {
public:
    virtual ~AdcRegistry() = default;

    // { Pre: none }
    //   isActive(adc)
    // { Post: true  => adc is registered and not decommissioned
    //         false => adc is unknown or decommissioned }
    virtual bool isActive(const AdcId& adc) const = 0;

    // { Pre: none }
    //   activeAdcs()
    // { Post: every registered, not decommissioned ADC, each once }
    virtual std::vector<AdcId> activeAdcs() const = 0;
};

}
