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

#include "message_routing.hpp"

#include <algorithm>
#include <cctype>
#include <exception>

namespace {

const char* const kHeartbeatKeyword = "Heartbeat";

bool parseAdcId(const std::string& token, int& out) {
    if (token.empty() || token.size() > 9) return false;
    for (char c : token) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    out = std::stoi(token);
    return out > 0;
}

} // namespace

MessageRouter::MessageRouter(EncryptionManager& encryption,
                             const PatientInfoStripper& stripper,
                             const ADClist& adcs,
                             EventScanner& scanner,
                             HeartbeatMonitor& heartbeats,
                             AlarmNotifier& alarms,
                             InventoryStore& inventory,
                             const Clock& clock)
    : encryption_(encryption), stripper_(stripper), adcs_(adcs), scanner_(scanner),
      heartbeats_(heartbeats), alarms_(alarms), inventory_(inventory), clock_(clock) {}

std::vector<std::string> MessageRouter::tokenize(const std::string& message, char delimiter) {
    std::vector<std::string> tokens;
    std::string current;
    for (char c : message) {
        if (c == delimiter) {
            tokens.push_back(current);
            current.clear();
        } else if (c != '\r' && c != '\n') {
            current += c;
        }
    }
    tokens.push_back(current);
    return tokens;
}

MessageRouter::Outcome MessageRouter::routePacket(const std::vector<uint8_t>& encryptedPacket) {
    std::vector<uint8_t> plain;
    try {
        plain = encryption_.decryptADCMessage(encryptedPacket);
    } catch (const std::exception&) {
        return Outcome::Malformed;   // empty, tampered with, or wrong key
    }
    const std::string message(plain.begin(), plain.end());
    // Patient information never leaves this function.
    return routeMessage(stripper_.strip(message));
}

MessageRouter::Outcome MessageRouter::routeMessage(const std::string& strippedMessage) {
    std::vector<std::string> tokens = tokenize(strippedMessage);
    if (tokens.size() < 2) return Outcome::Malformed;

    int adcId = 0;
    if (!parseAdcId(tokens[0], adcId)) return Outcome::Malformed;
    if (!adcs_.exists(adcId)) return Outcome::UnknownAdc;

    tokens.erase(tokens.begin());   // the rest of the message, without the ADC id

    if (tokens[0] == kHeartbeatKeyword) {
        heartbeats_.heartbeat(adcId, clock_.now());
        return Outcome::Heartbeat;
    }

    std::optional<Event> event = scanner_.scan(adcId, tokens);   // also logs it
    if (!event) return Outcome::NotAnEvent;

    switch (event->type) {
        case EventType::Alarm:
            alarms_.raise(adcId, event->fields.at("code"), event->fields.at("description"));
            break;
        case EventType::Withdrawal:
        case EventType::Deposit:
            updateInventory(*event);
            break;
        default:
            break;
    }
    return Outcome::Event;
}

std::size_t MessageRouter::pollLink(CommsLink& link) {
    std::size_t routed = 0;
    while (std::optional<std::vector<uint8_t>> packet = link.receive()) {
        routePacket(*packet);
        ++routed;
    }
    return routed;
}

void MessageRouter::updateInventory(const Event& event) {
    const std::string& medication = event.fields.at("medication");
    const int quantity = std::stoi(event.fields.at("quantity"));   // validated by event_contents
    const int current = inventory_.hasMedication(event.adcId, medication)
                            ? inventory_.count(event.adcId, medication)
                            : 0;

    if (event.type == EventType::Deposit) {
        inventory_.setCount(event.adcId, medication, current + quantity);
        return;
    }

    // Withdrawal. The ADC has already dispensed, so the count is updated even
    // if it disagrees with the HMS; a disagreement is raised as an alarm.
    // The alarm goes through event_contents like an ADC alarm so it is logged.
    if (quantity > current) {
        const std::string description = "Withdrawal of " + std::to_string(quantity) + " " +
                                        medication + " but HMS count was " +
                                        std::to_string(current);
        if (scanner_.scan(event.adcId, {"Alarm", "DISCREPANCY", description})) {
            alarms_.raise(event.adcId, "DISCREPANCY", description);
        }
    }
    inventory_.setCount(event.adcId, medication, std::max(0, current - quantity));
}
