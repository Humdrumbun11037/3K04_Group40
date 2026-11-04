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
    ConnectionLoss
};

// Ordered lowest to highest, so severities can be compared with < and >.
enum class Severity { Low, High, Critical };

enum class SeverityVerdict { Ok, UnknownType };

struct SeverityResult {
    SeverityVerdict verdict;
    Severity severity;   // only meaningful when verdict == Ok
};

class AlarmSeverity {
public:
    // { Pre: none (an unknown type is rejected, not assumed away) }
    //   severityOf(type)
    // { Post: type is in the table      => verdict Ok, severity from the table
    //         type is not in the table  => verdict UnknownType, never guessed }
    SeverityResult severityOf(AlarmType type) const;
};
