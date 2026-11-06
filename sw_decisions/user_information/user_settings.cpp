//user settings

/*
Hides: what each user is allowed to do

Receives: N/A
Sends: the information to initialize (and to authorization, which checks it
       on every user action)
*/

#include "user_settings.hpp"

#include <algorithm>
#include <cctype>

UserSettings::UserSettings() {
    // Technician permissions:
    //   look after the ADCs and respond to the alarms routed to them
    technician_ = {UserRole::Technician, "Technician", {
        Permission::ViewADCs,
        Permission::ViewInventory,
        Permission::ResolveAlarms,
    }};

    // Pharmacist permissions:
    //   own the medication stock and its reports
    pharmacist_ = {UserRole::Pharmacist, "Pharmacist", {
        Permission::ViewADCs,
        Permission::ViewInventory,
        Permission::ManualInventoryChange,
        Permission::GenerateReports,
        Permission::ResolveAlarms,
    }};

    // Administrator permissions:
    //   ADC management:
    //     Register new ADCs with new id and location
    //     Relocate existing ADCs
    //     Remove ADCs from the network
    //   User management: add / remove users, unlock locked accounts
    //   Maintenance scheduling, reports, alarms
    administrator_ = {UserRole::Administrator, "Administrator", {
        Permission::RegisterADC,
        Permission::RelocateADC,
        Permission::RemoveADC,
        Permission::ViewADCs,
        Permission::ManageUsers,
        Permission::UnlockAccounts,
        Permission::ScheduleMaintenance,
        Permission::ViewInventory,
        Permission::GenerateReports,
        Permission::ResolveAlarms,
    }};
}

const RoleSettings& UserSettings::settingsFor(UserRole role) const {
    switch (role) {
        case UserRole::Technician:    return technician_;
        case UserRole::Pharmacist:    return pharmacist_;
        case UserRole::Administrator: return administrator_;
    }
    return technician_;   // unreachable; least privileged as a fallback
}

bool UserSettings::isAllowed(UserRole role, Permission permission) const {
    const auto& allowed = settingsFor(role).permissions;
    return allowed.find(permission) != allowed.end();
}

std::vector<UserRole> UserSettings::allRoles() const {
    return {UserRole::Technician, UserRole::Pharmacist, UserRole::Administrator};
}

std::string UserSettings::roleName(UserRole role) {
    switch (role) {
        case UserRole::Technician:    return "Technician";
        case UserRole::Pharmacist:    return "Pharmacist";
        case UserRole::Administrator: return "Administrator";
    }
    return "Unknown";
}

bool UserSettings::roleFromName(const std::string& name, UserRole& out) {
    std::string lower(name);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower == "technician")    { out = UserRole::Technician;    return true; }
    if (lower == "pharmacist")    { out = UserRole::Pharmacist;    return true; }
    if (lower == "administrator") { out = UserRole::Administrator; return true; }
    return false;
}

std::string UserSettings::permissionName(Permission permission) {
    switch (permission) {
        case Permission::RegisterADC:           return "Register ADC";
        case Permission::RelocateADC:           return "Relocate ADC";
        case Permission::RemoveADC:             return "Remove ADC";
        case Permission::ViewADCs:              return "View ADCs";
        case Permission::ManageUsers:           return "Manage users";
        case Permission::UnlockAccounts:        return "Unlock accounts";
        case Permission::ScheduleMaintenance:   return "Schedule maintenance";
        case Permission::ViewInventory:         return "View inventory";
        case Permission::ManualInventoryChange: return "Manual inventory change";
        case Permission::GenerateReports:       return "Generate reports";
        case Permission::ResolveAlarms:         return "Resolve alarms";
    }
    return "Unknown";
}
