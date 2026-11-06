//alarm notification

/*
Hides: details of how alarms are routed

Receives: alarm message with severity decided from alarm severity
          unresolved flag from resolve alarm
Sends: alarm notification package to user dashboard through user registry
*/

#include "alarm_notification.hpp"

#include <algorithm>

AlarmNotifier::AlarmNotifier(const AlarmSeverityRules& severityRules,
                             AlarmResolver& resolver,
                             const UserRegistry& users,
                             DashboardOutput& dashboard,
                             EventRecorder& recorder,
                             const Clock& clock)
    : severityRules_(severityRules), resolver_(resolver), users_(users),
      dashboard_(dashboard), recorder_(recorder), clock_(clock) {}

// When given an alarm, check its type, reference the following chart:
//
//   Low:      technician
//   High:     pharmacist
//   Critical: administrator, technician
std::vector<UserRole> AlarmNotifier::recipientsFor(AlarmSeverity severity) {
    switch (severity) {
        case AlarmSeverity::Low:      return {UserRole::Technician};
        case AlarmSeverity::High:     return {UserRole::Pharmacist};
        case AlarmSeverity::Critical: return {UserRole::Administrator, UserRole::Technician};
    }
    return {UserRole::Administrator};
}

std::string AlarmNotifier::severityName(AlarmSeverity severity) {
    switch (severity) {
        case AlarmSeverity::Low:      return "LOW";
        case AlarmSeverity::High:     return "HIGH";
        case AlarmSeverity::Critical: return "CRITICAL";
    }
    return "UNKNOWN";
}

int AlarmNotifier::raise(int adcId, const std::string& code, const std::string& description) {
    AlarmRecord alarm{nextId_++, adcId, code, description,
                      severityRules_.classify(code), clock_.now()};

    resolver_.track(alarm.id);
    pending_.push_back(alarm);

    const std::string message = package(alarm, false);
    for (UserRole role : recipientsFor(alarm.severity)) {
        notifyRole(role, message);
    }
    return alarm.id;
}

// If an alarm isn't resolved in six minutes, send it to an administrator again
std::vector<int> AlarmNotifier::checkUnresolved() {
    const std::string now = clock_.now();
    std::vector<int> escalated;

    auto done = [&](const AlarmRecord& alarm) {
        if (resolver_.isResolved(alarm.id)) return true;
        if (timeutil::secondsBetween(alarm.raisedAt, now) < kEscalationSeconds) return false;

        notifyRole(UserRole::Administrator, package(alarm, true));

        Event event;
        event.type = EventType::AlarmEscalated;
        event.adcId = alarm.adcId;
        event.timestamp = now;
        event.fields["alarm_id"] = std::to_string(alarm.id);
        event.fields["code"] = alarm.code;
        event.fields["severity"] = severityName(alarm.severity);
        recorder_.record(event);

        escalated.push_back(alarm.id);
        return true;
    };

    pending_.erase(std::remove_if(pending_.begin(), pending_.end(), done), pending_.end());
    return escalated;
}

std::vector<AlarmRecord> AlarmNotifier::pending() const {
    std::vector<AlarmRecord> open;
    for (const AlarmRecord& alarm : pending_) {
        if (!resolver_.isResolved(alarm.id)) open.push_back(alarm);
    }
    return open;
}

void AlarmNotifier::notifyRole(UserRole role, const std::string& message) {
    std::vector<std::string> recipients = users_.usersWithRole(role);

    // Nobody holds that role: the alarm must still reach someone.
    if (recipients.empty() && role != UserRole::Administrator) {
        recipients = users_.usersWithRole(UserRole::Administrator);
    }
    for (const std::string& userId : recipients) {
        dashboard_.deliver(userId, message);
    }
}

std::string AlarmNotifier::package(const AlarmRecord& alarm, bool escalation) {
    std::string text;
    if (escalation) text += "[UNRESOLVED > 6 MIN] ";
    text += "[ALARM #" + std::to_string(alarm.id) + "][" + severityName(alarm.severity) + "] ";
    text += "ADC " + std::to_string(alarm.adcId) + " - " + alarm.code;
    if (!alarm.description.empty()) text += ": " + alarm.description;
    text += " (raised " + alarm.raisedAt + ")";
    return text;
}
