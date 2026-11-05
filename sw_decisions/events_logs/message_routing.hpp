//message routing

/*
Hides: the order in which an incoming ADC message is processed, and which
       module each kind of message is handed to

Receives: encrypted packets from the comms link
Sends: packets to encryption (decrypt), then to patient_info_stripping,
       heartbeats to adc_heartbeats,
       all other messages to event_contents,
       alarms to alarm_notification,
       withdrawals / deposits to the inventory record
*/
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../adc_registry.hpp"
#include "../alarm_notification.hpp"
#include "../external_interfaces.hpp"
#include "../message_processing/encryption.hpp"
#include "../message_processing/patient_info_stripping.hpp"
#include "event_contents.hpp"

class MessageRouter {
public:
    MessageRouter(EncryptionManager& encryption,
                  const PatientInfoStripper& stripper,
                  const ADClist& adcs,
                  EventScanner& scanner,
                  HeartbeatMonitor& heartbeats,
                  AlarmNotifier& alarms,
                  InventoryStore& inventory,
                  const Clock& clock);

    enum class Outcome {
        Heartbeat,        // passed to adc_heartbeats
        Event,            // logged (and alarm raised / inventory updated if needed)
        UnknownAdc,       // sender is not in the ADC registry; dropped
        Malformed,        // could not be decrypted or parsed; dropped
        NotAnEvent        // well formed, but contains no event; dropped
    };

    // Full pipeline for one packet: decrypt -> strip -> route.
    Outcome routePacket(const std::vector<uint8_t>& encryptedPacket);

    // Route a message that has already been decrypted and stripped.
    Outcome routeMessage(const std::string& strippedMessage);

    // Routes every packet waiting on the link. Returns how many were routed.
    std::size_t pollLink(CommsLink& link);

    static std::vector<std::string> tokenize(const std::string& message, char delimiter = '|');

private:
    void updateInventory(const Event& event);

    EncryptionManager& encryption_;
    const PatientInfoStripper& stripper_;
    const ADClist& adcs_;
    EventScanner& scanner_;
    HeartbeatMonitor& heartbeats_;
    AlarmNotifier& alarms_;
    InventoryStore& inventory_;
    const Clock& clock_;
};
