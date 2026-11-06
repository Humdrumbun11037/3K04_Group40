//manual inventory changes

/*
Hides: the process for implementing manual inventory changes

Receives: the user command for performing a manual inventory change from the user dashboard
Sends: updated inventory counts to the inventory record,
       a message to the user if the change was rejected,
       the change to report_generation
*/
#pragma once

#include <string>
#include <vector>

#include "adc_registry.hpp"
#include "external_interfaces.hpp"
#include "report_generation.hpp"

struct InventoryChange {
    std::string medication;
    int delta;   // positive adds stock, negative removes it
};

class ManualInventoryChanges {
public:
    ManualInventoryChanges(InventoryStore& inventory, const ADClist& adcs,
                           DashboardOutput& dashboard, EventRecorder& recorder,
                           const Clock& clock);

    struct Result {
        bool ok = false;
        std::string reason;   // why the change was rejected (empty if ok)
    };

    // All changes are applied together, or none are.
    Result apply(const std::string& userId, int adcId, const std::vector<InventoryChange>& changes);

private:
    InventoryStore& inventory_;
    const ADClist& adcs_;
    DashboardOutput& dashboard_;
    EventRecorder& recorder_;
    const Clock& clock_;
};
