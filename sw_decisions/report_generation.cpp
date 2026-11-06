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

#include "report_generation.hpp"

#include <stdexcept>

#include "message_processing/encryption.hpp"

ReportGenerator::ReportGenerator(AuditTrail& storage, const ReportTypes& types, const Clock& clock)
    : storage_(storage), types_(types), clock_(clock) {}

void ReportGenerator::record(const Event& event) {
    if (event.timestamp.empty()) {
        Event stamped = event;
        stamped.timestamp = clock_.now();
        storage_.append(stamped);
        return;
    }
    storage_.append(event);
}

ReportGenerator::Result ReportGenerator::generate(const std::string& reportType,
                                                  const std::string& from,
                                                  const std::string& to) const {
    if (!types_.isKnownType(reportType)) {
        return {false, "Unknown report type: " + reportType};
    }
    if (!timeutil::isValid(from) || !timeutil::isValid(to)) {
        return {false, "Times must be written as YYYY-MM-DD HH:MM:SS."};
    }
    if (to < from) {
        return {false, "The end of the report period is before the start."};
    }

    std::vector<Event> selected;
    for (const Event& event : storage_.search(from, to)) {
        if (types_.includes(reportType, event)) selected.push_back(event);
    }
    return {true, types_.format(reportType, from, to, selected)};
}

std::vector<uint8_t> ReportGenerator::generateEncrypted(const std::string& reportType,
                                                        const std::string& from,
                                                        const std::string& to,
                                                        EncryptionManager& encryption) const {
    const Result report = generate(reportType, from, to);
    if (!report.ok) throw std::invalid_argument(report.text);
    return encryption.encryptForReport(std::vector<uint8_t>(report.text.begin(), report.text.end()));
}
