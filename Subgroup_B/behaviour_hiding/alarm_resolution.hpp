//alarm resolution

/*
Hides: the rules for handling an alarm once it is raised (FR-ALM-02, FR-ALM-06,
       NFR-SAF-02, NFR-PRF-04): its states (Raised -> Acknowledged -> Resolved),
       who may acknowledge or resolve it, that resolving needs a note, the
       escalation time limits, and the supervisor chain

Interface
takes: new alarms, acknowledgements, resolutions with a note, requests to
       escalate overdue alarms
sends: each alarm's current details, the escalations made, entries to history
uses: alarm severity, clock, history

Invariants:
- an alarm is Acknowledged or Resolved only if that action was stored in history
- an alarm escalates at most once, and only while it is Raised (unacknowledged)
- a Resolved alarm never changes again
- each ADC has at most one unresolved alarm per type and detail (FR-ALM-04)
*/
#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "behaviour_hiding/alarm_severity.hpp"
#include "behaviour_hiding/interfaces/adc_id.hpp"
#include "behaviour_hiding/interfaces/clock.hpp"
#include "behaviour_hiding/interfaces/history.hpp"
#include "behaviour_hiding/interfaces/user.hpp"

using AlarmId = int;

enum class AlarmState { Raised, Acknowledged, Resolved };

enum class AddVerdict { Added, JoinedOpenAlarm, UnknownType };
enum class AcknowledgeVerdict { Acknowledged, NotAuthorized, UnknownAlarm, NotRaised, StorageFailure };
enum class ResolveVerdict { Resolved, NotAuthorized, UnknownAlarm, AlreadyResolved, NoExplanation,
                            StorageFailure };

struct AddResult {
    AddVerdict verdict;
    AlarmId id;   // the new or joined alarm; only meaningful when not UnknownType
};

// A copy of one alarm's details, for callers to read.
struct AlarmInfo {
    hms::behaviour::AdcId adc;
    AlarmType type;
    std::string detail;                 // e.g. the medication for InventoryLow; may be empty
    Severity severity;
    hms::behaviour::Role destination;   // the role the alarm is currently sent to
    AlarmState state;
    bool escalated;
    long long raisedAt;
};

// One escalation made by escalateDue(), for alarm notification to deliver.
struct Escalation {
    AlarmId id;
    hms::behaviour::Role destination;
    Severity severity;
};

class AlarmResolution {
public:
    // { Post: no alarms }
    AlarmResolution(const hms::behaviour::Clock& clock, hms::behaviour::History& history);

    // { Pre: none }
    //   add(adc, type, destination, detail)
    // { Post: the first matching row decides the result:
    //         1. type not in the severity table          => UnknownType, nothing added
    //         2. adc already has an unresolved alarm of
    //            this type and detail (FR-ALM-04)        => JoinedOpenAlarm, that alarm's id
    //         3. otherwise                               => Added, new alarm with a new id,
    //                                                       state Raised, severity from the
    //                                                       table, raised at clock now }
    AddResult add(const hms::behaviour::AdcId& adc, AlarmType type, hms::behaviour::Role destination,
                  const std::string& detail = "");

    // { Pre: none }
    //   acknowledge(user, id)
    // { Post: the first matching row decides the result:
    //         1. user's role may not handle alarms   => NotAuthorized
    //         2. no alarm with this id               => UnknownAlarm
    //         3. alarm is not Raised                 => NotRaised
    //         4. history write fails                 => StorageFailure
    //         5. otherwise                           => Acknowledged, recorded in history;
    //                                                   the alarm will not escalate
    //         Rows 1-4 change nothing. }
    AcknowledgeVerdict acknowledge(const hms::behaviour::User& user, AlarmId id);

    // { Pre: none }
    //   resolve(user, id, note)
    // { Post: the first matching row decides the result:
    //         1. user's role may not handle alarms   => NotAuthorized
    //         2. no alarm with this id               => UnknownAlarm
    //         3. alarm is already Resolved           => AlreadyResolved
    //         4. note is empty or only whitespace    => NoExplanation
    //         5. history write fails                 => StorageFailure
    //         6. otherwise                           => Resolved, history records who and why;
    //                                                   resolving a Raised alarm counts as
    //                                                   acknowledging it
    //         Rows 1-5 change nothing. }
    ResolveVerdict resolve(const hms::behaviour::User& user, AlarmId id, const std::string& note);

    // { Pre: none }
    //   escalateDue()
    // { Post: every alarm that is Raised, not yet escalated, and older than its
    //         severity's limit is escalated once:
    //         1. destination has a supervisor  => rerouted to the supervisor
    //         2. no supervisor                 => severity one level higher
    //                                             (Critical stays Critical)
    //         returns the escalations made }
    std::vector<Escalation> escalateDue();

    // { Pre: none }
    //   find(id)
    // { Post: the alarm's details, or nothing if there is no alarm with this id }
    std::optional<AlarmInfo> find(AlarmId id) const;

private:
    bool store(const std::string& kind, const std::string& who, const std::string& detail,
               const hms::behaviour::AdcId& adc);

    const hms::behaviour::Clock& clock;
    hms::behaviour::History& history;
    AlarmSeverity severityTable;
    std::map<AlarmId, AlarmInfo> alarms;
    AlarmId nextId = 1;
};
