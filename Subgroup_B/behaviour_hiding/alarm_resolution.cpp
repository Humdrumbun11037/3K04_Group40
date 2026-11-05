//alarm resolution

#include "behaviour_hiding/alarm_resolution.hpp"

#include <cctype>

using namespace hms::behaviour;

// ---- The rules. Everything that would change if the policy changes is here. ----

// Escalation limits: how long an alarm may stay unacknowledged.
static const long long CRITICAL_ESCALATION_SECONDS = 2 * 60;
static const long long HIGH_ESCALATION_SECONDS = 10 * 60;
// Low alarms never escalate.

static std::optional<long long> escalationLimit(Severity severity) {
    switch (severity) {
        case Severity::Critical: return CRITICAL_ESCALATION_SECONDS;
        case Severity::High:     return HIGH_ESCALATION_SECONDS;
        case Severity::Low:      return std::nullopt;
    }
    return std::nullopt;
}

// Supervisor chain for escalation.
static std::optional<Role> supervisorOf(Role role) {
    switch (role) {
        case Role::Technician:    return Role::Administrator;
        case Role::Pharmacist:    return Role::Administrator;
        case Role::Administrator: return std::nullopt;
    }
    return std::nullopt;
}

// Spec escalation rows 2 and 3: not Critical => one level higher; Critical => same.
static Severity oneLevelHigher(Severity severity) {
    switch (severity) {
        case Severity::Low:      return Severity::High;
        case Severity::High:     return Severity::Critical;
        case Severity::Critical: return Severity::Critical;
    }
    return Severity::Critical;
}

// Alarm lifecycle as a transition table: one row per allowed transition.
// Any (state, event) pair not listed is invalid and leaves the state unchanged.
enum class AlarmEvent { Acknowledge, Resolve };

static AlarmState next(AlarmState state, AlarmEvent event) {
    struct Transition { AlarmState from; AlarmEvent event; AlarmState to; };
    static const Transition table[] = {
        {AlarmState::Raised,       AlarmEvent::Acknowledge, AlarmState::Acknowledged}, // T1
        {AlarmState::Acknowledged, AlarmEvent::Resolve,     AlarmState::Resolved},     // T2
        // T3: resolving a Raised alarm directly counts as an implicit acknowledgement.
        {AlarmState::Raised,       AlarmEvent::Resolve,     AlarmState::Resolved},     // T3
    };
    for (const Transition& t : table) {
        if (t.from == state && t.event == event) return t.to;
    }
    return state;
}

static bool isDueForEscalation(const AlarmInfo& alarm, long long now) {
    if (alarm.state != AlarmState::Raised || alarm.escalated) return false;
    std::optional<long long> limit = escalationLimit(alarm.severity);
    return limit.has_value() && now - alarm.raisedAt >= *limit;
}

// A note that is empty or only whitespace is not an explanation.
static bool isBlank(const std::string& text) {
    for (char c : text) {
        if (!std::isspace(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

static bool isOpen(AlarmState state) {
    return state != AlarmState::Resolved;
}

// e.g. "alarm 3 (ADC-1)", or "alarm 4 (Hospital)" for a hospital-wide alarm, so
// report lines say where the alarm was.
static std::string alarmName(AlarmId id, const AdcId& adc) {
    return "alarm " + std::to_string(id) + " (" + adcName(adc) + ")";
}

// ---- The operations ----

AlarmResolution::AlarmResolution(const Clock& clock, History& history, const Permissions& permissions)
    : clock(clock), history(history), permissions(permissions) {}

// TODO(Inodh): alarm notification should call add() whenever it raises an alarm,
// and choose the destination role (e.g. which role gets an AdcOffline alarm).
// Rows are checked in table order, so the first matching row wins.
AddResult AlarmResolution::add(const AdcId& adc, AlarmType type, Role destination,
                               const std::string& detail) {
    SeverityResult severity = severityTable.severityOf(type);
    if (severity.verdict != SeverityVerdict::Ok)      return {AddVerdict::UnknownType, 0};    // row 1
    for (const auto& [id, alarm] : alarms) {                                                  // row 2
        if (alarm.adc == adc && alarm.type == type && alarm.detail == detail && isOpen(alarm.state)) {
            return {AddVerdict::JoinedOpenAlarm, id};
        }
    }
    AlarmId id = nextId++;                                                                    // row 3
    alarms[id] = {adc, type, detail, severity.severity, destination, AlarmState::Raised, false,
                  clock.now()};
    return {AddVerdict::Added, id};
}

// Rows are checked in table order, so the first matching row wins.
AcknowledgeVerdict AlarmResolution::acknowledge(const User& user, AlarmId id) {
    auto found = alarms.find(id);
    if (!permissions.mayPerform(user.role, Action::AcknowledgeAlarm))
                                                      return AcknowledgeVerdict::NotAuthorized;  // row 1
    if (found == alarms.end())                        return AcknowledgeVerdict::UnknownAlarm;   // row 2
    AlarmInfo& alarm = found->second;
    AlarmState after = next(alarm.state, AlarmEvent::Acknowledge);
    if (after == alarm.state)                         return AcknowledgeVerdict::NotRaised;      // row 3
    if (!store("AlarmAcknowledged", user.name, alarmName(id, alarm.adc), alarm.adc))
                                                      return AcknowledgeVerdict::StorageFailure; // row 4
    alarm.state = after;                                                                         // row 5
    return AcknowledgeVerdict::Acknowledged;
}

// Rows are checked in table order, so the first matching row wins.
ResolveVerdict AlarmResolution::resolve(const User& user, AlarmId id, const std::string& note) {
    auto found = alarms.find(id);
    if (!permissions.mayPerform(user.role, Action::ResolveAlarm))
                                                      return ResolveVerdict::NotAuthorized;      // row 1
    if (found == alarms.end())                        return ResolveVerdict::UnknownAlarm;       // row 2
    AlarmInfo& alarm = found->second;
    AlarmState after = next(alarm.state, AlarmEvent::Resolve);
    if (after == alarm.state)                         return ResolveVerdict::AlreadyResolved;    // row 3
    if (isBlank(note))                                return ResolveVerdict::NoExplanation;      // row 4
    if (!store("AlarmResolved", user.name, alarmName(id, alarm.adc) + ": " + note, alarm.adc))
                                                      return ResolveVerdict::StorageFailure;     // row 5
    alarm.state = after;                                                                         // row 6
    return ResolveVerdict::Resolved;
}

// Escalation table, applied once to each overdue alarm.
// TODO(Inodh): the spec says escalation creates a *new* alarm; here the existing
// alarm is changed in place, so it keeps its id and can still be resolved once.
std::vector<Escalation> AlarmResolution::escalateDue() {
    std::vector<Escalation> made;
    long long now = clock.now();
    for (auto& [id, alarm] : alarms) {
        if (!isDueForEscalation(alarm, now)) continue;
        std::optional<Role> supervisor = supervisorOf(alarm.destination);
        if (supervisor.has_value()) alarm.destination = *supervisor;              // row 1
        else                        alarm.severity = oneLevelHigher(alarm.severity); // rows 2, 3
        alarm.escalated = true;
        // TODO(team): if this history write fails, the escalation still happens,
        // because getting the alarm to a supervisor matters more than the record.
        store("AlarmEscalated", "HMS", alarmName(id, alarm.adc), alarm.adc);
        made.push_back({id, alarm.destination, alarm.severity});
    }
    return made;
}

std::optional<AlarmInfo> AlarmResolution::find(AlarmId id) const {
    auto found = alarms.find(id);
    if (found == alarms.end()) return std::nullopt;
    return found->second;
}

bool AlarmResolution::store(const std::string& kind, const std::string& who,
                            const std::string& detail, const AdcId& adc) {
    return history.record({kind, who, detail, clock.now(), adc, ""}) == HistoryResult::Stored;
}
