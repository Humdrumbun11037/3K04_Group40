//alarm severity

/*
Hides: which severity each alarm type has (the FR-ALM-03 table)

Interface
takes: an alarm type
sends: its severity, or a rejection if the type is not in the table
uses: nothing
*/
#pragma once

enum class AlarmType {
    UnauthorizedAccess,
    AdcOffline,
    IncorrectMessageFormat,
    InventoryLow,
    ConnectionLoss,
    HospitalLowStock,      // one medication low across the whole hospital (raised for ADC id 0)
    InventoryDiscrepancy   // an ADC withdrew more than the HMS count
};

// Ordered lowest to highest, so severities can be compared with < and >.
enum class Severity { Low, High, Critical };

enum class SeverityVerdict { Ok, UnknownType };

struct SeverityResult {
    SeverityVerdict verdict;
    Severity severity;   // only meaningful when verdict == Ok
};

// Tells everyone how serious each kind of alarm is. Keeping the whole table here
// means changing a severity touches only this module.
class AlarmSeverity {
public:
    // Looks up one alarm type's severity, so staff can see which alarms to deal with first.
    // { Pre: none (an unknown type is rejected, not assumed away) }
    //   severityOf(type)
    // { Post: type is in the table      => verdict Ok, severity from the table
    //         type is not in the table  => verdict UnknownType, never guessed }
    SeverityResult severityOf(AlarmType type) const;
};
