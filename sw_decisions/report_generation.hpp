//report generation

/*
Hides: how events are logged to long term storage and how stored events are
       gathered into a report

Receives: events from event_contents, maintenance_scheduler,
          alarm_notification, manual_inventory_changes, adc_registry,
          authorization
          report requests (type + time period) from the user dashboard
Sends: events to long term storage (audit trail)
       finished reports to the user dashboard
*/
#pragma once

#include <string>
#include <vector>

#include "external_interfaces.hpp"

class EncryptionManager;

// What every event producer talks to. Kept abstract so producers can be
// tested without storage.
class EventRecorder {
public:
    virtual ~EventRecorder() = default;
    virtual void record(const Event& event) = 0;
};

class ReportGenerator : public EventRecorder {
public:
    ReportGenerator(AuditTrail& storage, const ReportTypes& types, const Clock& clock);

    // Logs one event. Fills in the timestamp if the producer left it empty.
    void record(const Event& event) override;

    struct Result {
        bool ok = false;
        std::string text;   // the report if ok, otherwise the reason it failed
    };

    // Builds a report of the given type covering [from, to].
    Result generate(const std::string& reportType,
                    const std::string& from,
                    const std::string& to) const;

    // Same as generate, but the report is encrypted before it is handed on
    // (e.g. for export off the HMS).
    std::vector<uint8_t> generateEncrypted(const std::string& reportType,
                                           const std::string& from,
                                           const std::string& to,
                                           EncryptionManager& encryption) const;

private:
    AuditTrail& storage_;
    const ReportTypes& types_;
    const Clock& clock_;
};
