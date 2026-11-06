//fake maintenance status

/*
Stands in for the maintenance module when testing ADC heartbeats: the test
simply says which ADCs are in maintenance. The time passed in is ignored.
*/
#pragma once

#include <set>

#include "behaviour_hiding/interfaces/maintenance_status.hpp"

class FakeMaintenanceStatus : public hms::behaviour::MaintenanceStatus {
public:
    bool isInMaintenance(const hms::behaviour::AdcId& adc, long long) override {
        return inMaintenance.count(adc) > 0;
    }

    void start(const hms::behaviour::AdcId& adc) { inMaintenance.insert(adc); }
    void end(const hms::behaviour::AdcId& adc) { inMaintenance.erase(adc); }

private:
    std::set<hms::behaviour::AdcId> inMaintenance;
};
