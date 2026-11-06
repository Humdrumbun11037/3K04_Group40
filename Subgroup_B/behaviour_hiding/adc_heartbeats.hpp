//adc heartbeats

/*
Hides: the offline rule (an ADC silent for 3 heartbeat periods of 60 s, outside
       maintenance, is offline) and how silence is tracked
       (FR-COM-04, FR-HLT-02, NFR-SAF-01)

Interface
takes: which ADCs to monitor, heartbeats from ADCs, a request to check them all
sends: each ADC's status, an AdcOffline alarm when an ADC goes offline, and an
       "AdcHealth" history entry whenever an ADC goes Offline or back Online
uses: clock, maintenance status, alarm notifier, history

Invariants:
- an ADC that is not monitored (never added, or decommissioned) never raises an alarm
- at most one alarm per offline spell: once an alarm is raised, nothing more is
  raised until a heartbeat brings the ADC back Online
- a silent ADC in maintenance is shown Offline, but raises no alarm until the
  window ends
- this module never sends anything to an ADC, so it cannot affect dispensing
- history entries never block a change or an alarm: a failed write is ignored
*/
#pragma once

#include <map>
#include <string>

#include "behaviour_hiding/interfaces/adc_id.hpp"
#include "behaviour_hiding/interfaces/alarm_notifier.hpp"
#include "behaviour_hiding/interfaces/clock.hpp"
#include "behaviour_hiding/interfaces/history.hpp"
#include "behaviour_hiding/interfaces/maintenance_status.hpp"

enum class AdcStatus { Online, Offline, NotMonitored };
enum class HeartbeatVerdict { Accepted, NotMonitored };

// Watches each ADC's regular "I'm still here" messages and notices when one goes
// quiet, so staff learn quickly that an ADC has lost contact with the HMS.
class AdcHeartbeats {
public:
    // { Post: no ADCs are monitored }
    AdcHeartbeats(const hms::behaviour::Clock& clock,
                  hms::behaviour::MaintenanceStatus& maintenance,
                  hms::behaviour::AlarmNotifier& notifier,
                  hms::behaviour::History& history);

    // Starts watching an ADC, for example when it is registered.
    // { Pre: adc is registered (checked by the caller, e.g. startup or the ADC registry) }
    //   monitor(adc)
    // { Post: adc is monitored, Online, and counts as heard from now;
    //         already monitored => no change }
    void monitor(const hms::behaviour::AdcId& adc);

    // Stops watching an ADC, for example when it is decommissioned, so it can never
    // cause a false alarm.
    // { Pre: none }
    //   stopMonitoring(adc)
    // { Post: adc is not monitored and will never raise an alarm here }
    void stopMonitoring(const hms::behaviour::AdcId& adc);

    // Called for every heartbeat; each one proves the ADC is still talking to us.
    // { Pre: the message was already validated by message processing }
    //   heartbeatReceived(adc)
    // { Post: row 1: adc monitored => missed count reset, status Online, Accepted;
    //                                 if it was Offline, history entry "back online"
    //         adc not monitored    => no change, NotMonitored }
    HeartbeatVerdict heartbeatReceived(const hms::behaviour::AdcId& adc);

    // Run on every tick: finds ADCs that have gone silent and raises the alarm,
    // unless the ADC is in planned maintenance.
    // { Pre: none }
    //   checkAll()
    // { Post: for every monitored ADC, the offline table in the .cpp is applied
    //         at clock now: an ADC silent for 3 periods becomes Offline; outside
    //         maintenance one AdcOffline alarm is raised for that silent spell;
    //         each change to Offline writes a history entry }
    void checkAll();

    // What the dashboard shows for one ADC: Online, Offline or not monitored.
    // { Pre: none }
    //   status(adc)
    // { Post: returns Online or Offline, or NotMonitored if adc is not monitored }
    AdcStatus status(const hms::behaviour::AdcId& adc) const;

private:
    struct Record {
        long long lastHeard;   // missed periods are counted from here
        AdcStatus status;      // Online or Offline only
        bool alarmRaised;      // an alarm was raised in the current offline spell
    };

    void checkAdc(const hms::behaviour::AdcId& adc, Record& record, long long now);
    void recordHealth(const hms::behaviour::AdcId& adc, const std::string& what);

    const hms::behaviour::Clock& clock;
    hms::behaviour::MaintenanceStatus& maintenance;
    hms::behaviour::AlarmNotifier& notifier;
    hms::behaviour::History& history;
    std::map<hms::behaviour::AdcId, Record> records;
};
