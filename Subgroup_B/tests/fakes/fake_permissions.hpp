//fake permissions

/*
The team's agreed permission table, for tests and the demo:
  ScheduleMaintenance, CancelMaintenance: Technician
  ManualInventoryChange:                  Pharmacist or Administrator
  AcknowledgeAlarm, ResolveAlarm:         all three roles
  GenerateSummary:                        Pharmacist or Administrator
A test can override one answer with set(role, action, allowed) to show that a
module follows whatever the table says.
*/
#pragma once

#include <map>
#include <utility>

#include "behaviour_hiding/interfaces/permissions.hpp"

class FakePermissions : public hms::behaviour::Permissions {
public:
    bool mayPerform(hms::behaviour::Role role, hms::behaviour::Action action) const override {
        using hms::behaviour::Action;
        using hms::behaviour::Role;
        auto overridden = overrides.find({role, action});
        if (overridden != overrides.end()) return overridden->second;

        bool pharmacist = role == Role::Pharmacist;
        bool technician = role == Role::Technician;
        bool administrator = role == Role::Administrator;
        switch (action) {
            case Action::ScheduleMaintenance:   return technician;
            case Action::CancelMaintenance:     return technician;
            case Action::ManualInventoryChange: return pharmacist || administrator;
            case Action::AcknowledgeAlarm:      return pharmacist || technician || administrator;
            case Action::ResolveAlarm:          return pharmacist || technician || administrator;
            case Action::GenerateSummary:       return pharmacist || administrator;
        }
        return false;
    }

    void set(hms::behaviour::Role role, hms::behaviour::Action action, bool allowed) {
        overrides[{role, action}] = allowed;
    }

private:
    std::map<std::pair<hms::behaviour::Role, hms::behaviour::Action>, bool> overrides;
};
