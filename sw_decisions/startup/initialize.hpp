//initialize

/*
Hides: the order in which the HMS modules are created and connected, and
       the checks made before the system starts

Receives: the role information from user_settings,
          the hw_hiding / behav_hiding modules (as interfaces),
          the encryption key
Sends: a fully connected system, ready for use by the user dashboard
*/
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "../adc_registry.hpp"
#include "../alarm_notification.hpp"
#include "../events_logs/event_contents.hpp"
#include "../events_logs/message_routing.hpp"
#include "../external_interfaces.hpp"
#include "../maintenance_scheduler.hpp"
#include "../manual_inventory_changes.hpp"
#include "../message_processing/encryption.hpp"
#include "../message_processing/patient_info_stripping.hpp"
#include "../report_generation.hpp"
#include "../user_information/user_registry.hpp"
#include "../user_information/user_settings.hpp"
#include "authorization.hpp"

// The modules outside sw_decisions that the HMS is built on.
struct ExternalModules {
    const Clock& clock;
    CommsLink& comms;
    DashboardOutput& dashboard;
    AuditTrail& storage;
    const AlarmSeverityRules& alarmSeverity;
    AlarmResolver& alarmResolution;
    const ReportTypes& reportTypes;
    InventoryStore& inventory;
    HeartbeatMonitor& heartbeats;
};

class HMSystem {
public:
    // Throws std::logic_error if the user settings are unusable, and
    // std::invalid_argument if the key is the wrong size.
    HMSystem(const ExternalModules& external,
             std::vector<uint8_t> encryptionKey,
             std::unique_ptr<IEncryptionStrategy> strategy = EncryptionManager::defaultStrategy());

    HMSystem(const HMSystem&) = delete;
    HMSystem& operator=(const HMSystem&) = delete;

    // Creates the first administrator. Only works while there are no
    // administrators, so it cannot be used to take over a running system.
    bool createInitialAdministrator(const std::string& userId, const std::string& password);

    // One pass of the main loop: route waiting ADC messages, escalate
    // unresolved alarms, and start / end maintenance windows.
    void tick();

    // Members are declared (and therefore built) in dependency order.
    const ExternalModules external;
    const UserSettings settings;
    UserRegistry users;
    ReportGenerator reports;
    ADClist adcs;
    EncryptionManager encryption;
    PatientInfoStripper stripper;
    EventScanner scanner;
    AlarmNotifier alarms;
    MessageRouter router;
    Authorization authorization;
    MaintenanceScheduler maintenance;
    ManualInventoryChanges inventoryChanges;

private:
    // Loads the user settings and checks them before anything else is built.
    static UserSettings loadCheckedSettings();
};
