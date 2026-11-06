//integrated: alarm adapters

/*
Hides: how an alarm passes between the two sides
  - Andrew's alarms are an AlarmType enum; Inodh's are text codes (table below)
  - Inodh's notification delivers each alarm to the right users' dashboards;
    Andrew's alarm resolution holds its state (acknowledge, resolve, escalate)

  NotifierBridge       Andrew's AlarmNotifier -> Inodh's AlarmNotifier::raise
                       (Andrew's modules raise alarms here)
  AlarmLinks::sync     Inodh's raised alarms -> Andrew's AlarmResolution::add,
                       for every alarm, including ones the router raises itself
  SeverityRules        Inodh's AlarmSeverityRules::classify <- Andrew's AlarmSeverity
  ResolverLink         Inodh's AlarmResolver::isResolved   <- Andrew's AlarmResolution

Inodh's own escalation (AlarmNotifier::checkUnresolved, 6 minutes) is never
called; Andrew's escalateDue() does escalation.
*/
#pragma once

#include "gui/integrated/convert.hpp"

#include <map>
#include <set>
#include <string>

#include "behaviour_hiding/alarm_resolution.hpp"
#include "behaviour_hiding/alarm_severity.hpp"
#include "behaviour_hiding/interfaces/alarm_notifier.hpp"
#include "gui/integrated/feed.hpp"

namespace alarm_codes {
// AlarmType <-> Inodh's alarm code. InventoryDiscrepancy uses his existing "DISCREPANCY".
std::string codeOf(AlarmType type);
bool typeOf(const std::string& code, AlarmType& type);
}  // namespace alarm_codes

class SeverityRules : public AlarmSeverityRules {
public:
    // A code with no matching type is Critical: unknown alarms err on the safe side.
    SwAlarmSeverity classify(const std::string& alarmCode) const override;

private:
    AlarmSeverity table;
};

// Which of Inodh's alarm ids is which of Andrew's, and the alarms that could not be matched.
class AlarmLinks {
public:
    AlarmLinks(AlarmResolution& alarms, const hms::behaviour::Clock& clock, Feed& feed)
        : alarms(alarms), clock(clock), feed(feed) {}

    // Gives every alarm Inodh's notifier has raised (and not yet seen here) to
    // alarm resolution: its type from the code, its first recipient role as the
    // destination, its description as the detail. Bounded by the pending list.
    void sync(const ::AlarmNotifier& notifier);

    bool isResolved(int swId) const;
    std::optional<AlarmId> mine(int swId) const;

    std::set<int> unmatched;   // Inodh's alarm ids whose code has no AlarmType

private:
    AlarmResolution& alarms;
    const hms::behaviour::Clock& clock;
    Feed& feed;
    std::map<int, AlarmId> links;
};

class ResolverLink : public AlarmResolver {
public:
    explicit ResolverLink(const AlarmLinks& links) : links(links) {}
    void track(int) override {}   // AlarmLinks::sync picks the alarm up after raise() returns
    bool isResolved(int alarmId) const override { return links.isResolved(alarmId); }

private:
    const AlarmLinks& links;
};

class NotifierBridge : public hms::behaviour::AlarmNotifier {
public:
    NotifierBridge(::AlarmNotifier& notifier, AlarmLinks& links) : notifier(notifier), links(links) {}

    void raise(const hms::behaviour::AdcId& adc, AlarmType type, const std::string& detail = "") override {
        notifier.raise(convert::toSwAdc(adc), alarm_codes::codeOf(type), detail);
        links.sync(notifier);
    }

private:
    ::AlarmNotifier& notifier;
    AlarmLinks& links;
};
