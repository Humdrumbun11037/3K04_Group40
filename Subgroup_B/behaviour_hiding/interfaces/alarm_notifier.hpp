//alarm notifier (interface only)

/*
Owned by alarm notification (software decisions). This file only states what the
behaviour modules need from it: a way to raise an alarm. The notifier looks up
the severity itself (using alarm severity) and decides who is told.

Interface
takes: the ADC, the alarm type, and an optional detail (e.g. the medication for
       InventoryLow); empty detail means none
*/
#pragma once

#include <string>

#include "behaviour_hiding/alarm_severity.hpp"
#include "behaviour_hiding/interfaces/adc_id.hpp"

namespace hms::behaviour {

class AlarmNotifier {
public:
    virtual ~AlarmNotifier() = default;

    // { Pre: none }
    //   raise(adc, type, detail)
    // { Post: an alarm of this type, with this detail, for this ADC has been raised }
    // Overrides repeat the same default, so a call with two arguments works on any type.
    virtual void raise(const AdcId& adc, AlarmType type, const std::string& detail = "") = 0;
};

}
