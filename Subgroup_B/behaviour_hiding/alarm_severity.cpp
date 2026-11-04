//alarm severity

#include "behaviour_hiding/alarm_severity.hpp"

// One case per row of the FR-ALM-03 table, in table order.
// No default case: if a new AlarmType is added without a row, -Wall warns.
SeverityResult AlarmSeverity::severityOf(AlarmType type) const {
    switch (type) {
        case AlarmType::UnauthorizedAccess:     return {SeverityVerdict::Ok, Severity::Critical}; // row 1
        case AlarmType::AdcOffline:             return {SeverityVerdict::Ok, Severity::Critical}; // row 2
        case AlarmType::IncorrectMessageFormat: return {SeverityVerdict::Ok, Severity::Critical}; // row 3
        case AlarmType::InventoryLow:           return {SeverityVerdict::Ok, Severity::High};     // row 4
        case AlarmType::ConnectionLoss:         return {SeverityVerdict::Ok, Severity::Low};      // row 5
    }
    // Not in the table: reject. The severity field is filler; Critical so that a
    // caller who forgets to check the verdict errs on the safe side.
    return {SeverityVerdict::UnknownType, Severity::Critical};
}
