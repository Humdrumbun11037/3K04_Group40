//user settings

/*
Hides: what each user is allowed to do

Receives: N/A
Sends: the information to initialize (and to authorization, which checks it
       on every user action)
*/
#pragma once

#include <set>
#include <string>
#include <vector>

enum class UserRole { Technician, Pharmacist, Administrator };

enum class Permission {
    // ADC management
    RegisterADC,
    RelocateADC,
    RemoveADC,
    ViewADCs,
    // User management
    ManageUsers,
    UnlockAccounts,
    // Maintenance
    ScheduleMaintenance,
    // Inventory
    ViewInventory,
    ManualInventoryChange,
    // Reports
    GenerateReports,
    // Alarms
    ResolveAlarms
};

// One of these exists for each type of user.
struct RoleSettings {
    UserRole role;
    std::string name;
    std::set<Permission> permissions;
};

class UserSettings {
public:
    UserSettings();

    const RoleSettings& settingsFor(UserRole role) const;
    bool isAllowed(UserRole role, Permission permission) const;
    std::vector<UserRole> allRoles() const;

    static std::string roleName(UserRole role);
    // Case-insensitive. Returns false if the name is not a role.
    static bool roleFromName(const std::string& name, UserRole& out);
    static std::string permissionName(Permission permission);

private:
    RoleSettings technician_;
    RoleSettings pharmacist_;
    RoleSettings administrator_;
};
