//gui names

/*
Hides: the display name of every behaviour-module enum value

Interface
takes: an enum value
sends: its name, exactly as written in the module's header (e.g. "NoExplanation")
uses: the behaviour modules' headers (types only)

Each list is in the same order as its enum, as in demo_main.cpp.
*/
#pragma once

#include <cstddef>

#include "behaviour_hiding/adc_heartbeats.hpp"
#include "behaviour_hiding/alarm_resolution.hpp"
#include "behaviour_hiding/hospital_stock.hpp"
#include "behaviour_hiding/inventory_record.hpp"
#include "behaviour_hiding/maintenance.hpp"
#include "behaviour_hiding/report_types.hpp"

template <typename E, std::size_t N>
const char* nameOf(E value, const char* const (&names)[N]) {
    std::size_t i = static_cast<std::size_t>(value);
    return i < N ? names[i] : "?";
}

namespace names {
inline const char* const SEVERITY[] = {"Low", "High", "Critical"};
inline const char* const ALARM_TYPE[] = {"UnauthorizedAccess", "AdcOffline", "IncorrectMessageFormat",
                                         "InventoryLow", "ConnectionLoss", "HospitalLowStock",
                                         "InventoryDiscrepancy"};
inline const char* const ROLE[] = {"Pharmacist", "Technician", "Administrator"};
inline const char* const ALARM_STATE[] = {"Raised", "Acknowledged", "Resolved"};
inline const char* const ADC_STATUS[] = {"Online", "Offline", "NotMonitored"};
inline const char* const HOSPITAL[] = {"NotLow", "Low", "UnknownMedication"};
inline const char* const RESERVE[] = {"Updated", "NotAuthorized", "UnknownMedication", "IncorrectValue",
                                      "NoExplanation", "StorageFailure"};
inline const char* const ADD[] = {"Added", "JoinedOpenAlarm", "UnknownType"};
inline const char* const ACK[] = {"Acknowledged", "NotAuthorized", "UnknownAlarm", "NotRaised",
                                  "StorageFailure"};
inline const char* const RESOLVE[] = {"Resolved", "NotAuthorized", "UnknownAlarm", "AlreadyResolved",
                                      "NoExplanation", "StorageFailure"};
inline const char* const SCHEDULE[] = {"Created", "NotAuthorized", "UnknownAdc", "InvalidWindow",
                                       "WindowTooLong", "Overlap"};
inline const char* const CANCEL[] = {"Cancelled", "NotAuthorized", "UnknownWindow", "NotCancellable"};
inline const char* const INVENTORY[] = {"Updated", "NotAuthorized", "UnknownItem", "IncorrectValue",
                                        "NoExplanation", "StorageFailure", "DiscrepancyRecorded"};
inline const char* const DAILY[] = {"Sent", "AlreadyDone", "TooEarly", "GenerationFailed", "SendFailed"};
inline const char* const SUMMARY[] = {"Summarized", "NotAuthorized", "InvalidRange", "ReadFailure",
                                      "AuditFailure"};
inline const char* const ACTION[] = {"ScheduleMaintenance", "CancelMaintenance", "ManualInventoryChange",
                                     "AcknowledgeAlarm", "ResolveAlarm", "GenerateSummary"};
}  // namespace names
