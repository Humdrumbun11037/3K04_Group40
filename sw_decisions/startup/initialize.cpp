//initialize

/*
Hides: the order in which the HMS modules are created and connected, and
       the checks made before the system starts

Receives: the role information from user_settings,
          the hw_hiding / behav_hiding modules (as interfaces),
          the encryption key
Sends: a fully connected system, ready for use by the user dashboard
*/

#include "initialize.hpp"

#include <stdexcept>

UserSettings HMSystem::loadCheckedSettings() {
    UserSettings loaded;

    // Every role must be able to do something, and administrators must be
    // able to manage users and unlock accounts or the system can never be
    // recovered from a lockout.
    for (UserRole role : loaded.allRoles()) {
        if (loaded.settingsFor(role).permissions.empty()) {
            throw std::logic_error("User settings: role " + UserSettings::roleName(role) +
                                   " has no permissions.");
        }
    }
    if (!loaded.isAllowed(UserRole::Administrator, Permission::ManageUsers) ||
        !loaded.isAllowed(UserRole::Administrator, Permission::UnlockAccounts)) {
        throw std::logic_error("User settings: administrators must be able to manage users.");
    }
    return loaded;
}

HMSystem::HMSystem(const ExternalModules& ext,
                   std::vector<uint8_t> encryptionKey,
                   std::unique_ptr<IEncryptionStrategy> strategy)
    : external(ext),
      settings(loadCheckedSettings()),
      users(),
      reports(ext.storage, ext.reportTypes, ext.clock),
      adcs(&reports),
      encryption(std::move(strategy), std::move(encryptionKey)),
      stripper(),
      scanner(ext.clock, reports),
      alarms(ext.alarmSeverity, ext.alarmResolution, users, ext.dashboard, reports, ext.clock),
      router(encryption, stripper, adcs, scanner, ext.heartbeats, alarms, ext.inventory, ext.clock),
      authorization(users, settings, reports, ext.clock),
      maintenance(ext.clock, reports),
      inventoryChanges(ext.inventory, adcs, ext.dashboard, reports, ext.clock),
      daily(ext.clock, ext.storage) {}

bool HMSystem::createInitialAdministrator(const std::string& userId, const std::string& password) {
    if (!users.usersWithRole(UserRole::Administrator).empty()) return false;
    return users.addUser(userId, password, UserRole::Administrator);
}

void HMSystem::tick() {
    router.pollLink(external.comms);
    alarms.checkUnresolved();
    maintenance.tick();
    daily.tick();
}
