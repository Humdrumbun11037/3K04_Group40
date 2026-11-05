//alarm notification

/*
Hides: details of how alarms are routed

Receives: alarm message with severity decided from alarm severity
          unresolved flag from resolve alarm
Sends: alarm notification package to user dashboard through user registry
*/
#pragma once

#include <string>
#include <vector>

#include "external_interfaces.hpp"
#include "report_generation.hpp"
#include "user_information/user_registry.hpp"

struct AlarmRecord {
    int id;
    int adcId;
    std::string code;
    std::string description;
    AlarmSeverity severity;
    std::string raisedAt;
};

class AlarmNotifier {
public:
    // An alarm that is still unresolved after this long is sent to the
    // administrators again.
    static constexpr long long kEscalationSeconds = 6 * 60;

    AlarmNotifier(const AlarmSeverityRules& severityRules,
                  AlarmResolver& resolver,
                  const UserRegistry& users,
                  DashboardOutput& dashboard,
                  EventRecorder& recorder,
                  const Clock& clock);

    // Classifies a new alarm, starts tracking it, and notifies the users
    // responsible for it. Returns the id given to the alarm.
    int raise(int adcId, const std::string& code, const std::string& description);

    // Call periodically. Re-sends every alarm that has gone unresolved for
    // kEscalationSeconds to the administrators (once per alarm).
    // Returns the ids of the alarms that were escalated.
    std::vector<int> checkUnresolved();

    // Alarms that are neither resolved nor escalated yet.
    std::vector<AlarmRecord> pending() const;

    // Roles that receive an alarm of the given severity.
    static std::vector<UserRole> recipientsFor(AlarmSeverity severity);
    static std::string severityName(AlarmSeverity severity);

private:
    void notifyRole(UserRole role, const std::string& message);
    static std::string package(const AlarmRecord& alarm, bool escalation);

    const AlarmSeverityRules& severityRules_;
    AlarmResolver& resolver_;
    const UserRegistry& users_;
    DashboardOutput& dashboard_;
    EventRecorder& recorder_;
    const Clock& clock_;

    int nextId_ = 1;
    std::vector<AlarmRecord> pending_;
};
