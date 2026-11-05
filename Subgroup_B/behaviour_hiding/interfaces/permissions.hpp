//permissions (interface only)

/*
Owned by user settings (software decisions). This file only states what the
behaviour modules need to ask: may a user with this role perform this action?
The table of answers lives with the owner, so the behaviour modules contain no
role rules of their own.

Interface
takes: a role and an action
sends: whether that role may perform that action
*/
#pragma once

#include "behaviour_hiding/interfaces/user.hpp"

namespace hms::behaviour {

enum class Action {
    ScheduleMaintenance,
    CancelMaintenance,
    ManualInventoryChange,
    AcknowledgeAlarm,
    ResolveAlarm,
    GenerateSummary
};

class Permissions {
public:
    virtual ~Permissions() = default;

    // { Pre: none }
    //   mayPerform(role, action)
    // { Post: true iff a user with this role may perform this action }
    virtual bool mayPerform(Role role, Action action) const = 0;
};

}
