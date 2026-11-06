//adc heartbeats

#include "behaviour_hiding/adc_heartbeats.hpp"

using namespace hms::behaviour;

// The offline rule. Changing it means changing only these two numbers.
static const long long PERIOD_SECONDS = 60;
static const long long MISSED_PERIODS_FOR_OFFLINE = 3;

AdcHeartbeats::AdcHeartbeats(const Clock& clock, MaintenanceStatus& maintenance,
                             AlarmNotifier& notifier, History& history)
    : clock(clock), maintenance(maintenance), notifier(notifier), history(history) {}

// Starting to watch counts as hearing from the ADC now, so a newly added ADC is
// not reported offline straight away.
void AdcHeartbeats::monitor(const AdcId& adc) {
    if (records.count(adc) > 0) return;
    records[adc] = {clock.now(), AdcStatus::Online, false};
}

void AdcHeartbeats::stopMonitoring(const AdcId& adc) {
    records.erase(adc);
}

// Row 1 of the offline table: a valid update resets the missed count.
HeartbeatVerdict AdcHeartbeats::heartbeatReceived(const AdcId& adc) {
    auto found = records.find(adc);
    if (found == records.end()) return HeartbeatVerdict::NotMonitored;
    bool wasOffline = found->second.status == AdcStatus::Offline;
    found->second = {clock.now(), AdcStatus::Online, false};                              // row 1
    if (wasOffline) recordHealth(adc, "back online");
    return HeartbeatVerdict::Accepted;
}

// Checks every watched ADC against the offline rule below.
void AdcHeartbeats::checkAll() {
    long long now = clock.now();
    for (auto& [adc, record] : records) {
        checkAdc(adc, record, now);
    }
}

AdcStatus AdcHeartbeats::status(const AdcId& adc) const {
    auto found = records.find(adc);
    if (found == records.end()) return AdcStatus::NotMonitored;
    return found->second.status;
}

// Offline table for one ADC, checked in order, first match wins.
// Spec rows 2 and 4 are the same case here: once a maintenance window ends,
// isInMaintenance turns false, so a still-silent ADC falls into row D.
//   A. fewer than 3 periods missed                => no change
//   B. 3+ missed, in maintenance                  => Offline, no alarm (spec row 3)
//   C. 3+ missed, alarm already raised this spell => no change, no second alarm (FR-ALM-04)
//   D. 3+ missed, not in maintenance, no alarm yet => Offline, raise alarm (spec rows 2 and 4)
// An ADC that went offline before maintenance began keeps its alarm open (row C
// once the window ends).
void AdcHeartbeats::checkAdc(const AdcId& adc, Record& record, long long now) {
    long long missed = (now - record.lastHeard) / PERIOD_SECONDS;
    if (missed < MISSED_PERIODS_FOR_OFFLINE) return;                                      // A
    bool wasOnline = record.status == AdcStatus::Online;
    if (maintenance.isInMaintenance(adc, now)) {                                          // B
        record.status = AdcStatus::Offline;
        if (wasOnline) recordHealth(adc, "offline during maintenance: no alarm");
        return;
    }
    if (record.alarmRaised) return;                                                       // C
    record.status = AdcStatus::Offline;                                                   // D
    record.alarmRaised = true;
    // TODO(Inodh): confirm heartbeats raise AdcOffline (Critical), not ConnectionLoss (Low).
    notifier.raise(adc, AlarmType::AdcOffline);
    if (wasOnline) recordHealth(adc, "offline: 3 heartbeat periods missed");
}

// Written after the change and the alarm; a failed write is ignored on purpose,
// so the history can never block either.
void AdcHeartbeats::recordHealth(const AdcId& adc, const std::string& what) {
    history.record({"AdcHealth", "HMS", adcName(adc) + " " + what, clock.now(), adc, ""});
}
