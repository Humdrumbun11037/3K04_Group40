//maintenance status (interface only)

/*
The one question ADC heartbeats asks the maintenance module. Heartbeats depend
on this interface, not on Maintenance itself, so they can be tested with a fake
and the maintenance rules can change without touching heartbeats.

Interface
takes: an ADC id and a time
sends: whether that ADC is in an active maintenance window at that time
*/
#pragma once

#include "behaviour_hiding/interfaces/adc_id.hpp"

namespace hms::behaviour {

class MaintenanceStatus {
public:
    virtual ~MaintenanceStatus() = default;

    // { Pre: none }
    //   isInMaintenance(adc, now)
    // { Post: true  => adc has a window that is Active at time now
    //         false => it does not }
    virtual bool isInMaintenance(const AdcId& adc, long long now) = 0;
};

}
