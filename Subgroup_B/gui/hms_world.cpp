//gui world

#include "gui/hms_world.hpp"

#include "gui/names.hpp"

using namespace hms::behaviour;

static const long long DAY_SECONDS = 24 * 60 * 60;
static const std::size_t FEED_LIMIT = 300;   // oldest items are dropped after this

void GuiNotifier::raise(const AdcId& adc, AlarmType type, const std::string& detail) {
    AddResult r = alarms.add(adc, type, destinationFor(type), detail);
    std::string text = std::string(nameOf(type, names::ALARM_TYPE)) + (detail.empty() ? "" : " (" + detail + ")")
                     + " for " + adcName(adc) + ": " + nameOf(r.verdict, names::ADD);
    if (r.verdict != AddVerdict::UnknownType) {
        AlarmInfo info = *alarms.find(r.id);
        text += " #" + std::to_string(r.id) + " (" + nameOf(info.severity, names::SEVERITY) + ", to "
              + nameOf(info.destination, names::ROLE) + ")";
    }
    feed.push_back({clock.now(), "alarm", text});
}

Role GuiNotifier::destinationFor(AlarmType type) {
    switch (type) {
        case AlarmType::InventoryLow:         return Role::Pharmacist;
        case AlarmType::HospitalLowStock:     return Role::Pharmacist;
        case AlarmType::InventoryDiscrepancy: return Role::Pharmacist;
        case AlarmType::UnauthorizedAccess:   return Role::Administrator;
        default:                              return Role::Technician;
    }
}

GuiWorld::GuiWorld() {
    addAdc(1);
    addAdc(2);
    // A day of heartbeats, one a minute, so the daily report flags only ADCs
    // that go silent during the demo.
    for (const AdcId& adc : registry.activeAdcs()) {
        for (long long t = START_TIME - DAY_SECONDS + HEARTBEAT_SECONDS; t <= START_TIME;
             t += HEARTBEAT_SECONDS) {
            heartbeatLog.record(adc, t);
        }
        lastHeartbeat[adc] = START_TIME;
    }
    inventory.addItem(1, "Morphine", 40, 100);
    inventory.addItem(1, "Insulin", 30, 100);
    inventory.addItem(2, "Morphine", 60, 100);
    // Made-up reserve levels: Insulin is just above 25% across the hospital
    // (160 of 600), so a reserve change to 20 makes it low while ADC-1 stays fine.
    hospital.addReserve("Morphine", 300, 400);
    hospital.addReserve("Insulin", 130, 500);
    hospital.addReserve("Heparin", 150, 200);
    note("action", "HMS started: ADC-1 and ADC-2 online; user Tara (Technician)");
}

void GuiWorld::addAdc(const AdcId& adc) {
    registry.add(adc);
    heartbeats.monitor(adc);
    shownStatus[adc] = AdcStatus::Online;
    lastHeartbeat[adc] = clock.now();
}

void GuiWorld::decommission(const AdcId& adc) {
    registry.decommission(adc);
    heartbeats.stopMonitoring(adc);
    shownStatus.erase(adc);
    silenced.erase(adc);
    lastHeartbeat.erase(adc);
}

void GuiWorld::sendHeartbeat(const AdcId& adc) {
    if (heartbeats.heartbeatReceived(adc) == HeartbeatVerdict::Accepted) {
        heartbeatLog.record(adc, clock.now());
        lastHeartbeat[adc] = clock.now();
    }
}

void GuiWorld::advance(long long seconds) {
    while (seconds > 0) {
        long long step = seconds < TICK_SECONDS ? seconds : TICK_SECONDS;
        long long minuteBefore = clock.now() / HEARTBEAT_SECONDS;
        clock.advance(step);
        seconds -= step;
        if (clock.now() / HEARTBEAT_SECONDS != minuteBefore) {
            for (const AdcId& adc : registry.activeAdcs()) {
                if (silenced.count(adc) == 0) sendHeartbeat(adc);
            }
        }
        periodicChecks();
    }
}

void GuiWorld::periodicChecks() {
    heartbeats.checkAll();
    hospital.checkAll();
    noteStatusChanges();
    for (const Escalation& e : alarms.escalateDue()) {
        note("escalation", "Alarm #" + std::to_string(e.id) + " escalated: now to "
                           + nameOf(e.destination, names::ROLE) + ", " + nameOf(e.severity, names::SEVERITY));
    }
    // Only changes are worth showing; "too early" and "already done" repeat every tick.
    DailyVerdict v = reports.generateDaily(clock.now());
    if (v == DailyVerdict::Sent || (v != lastDaily && v != DailyVerdict::AlreadyDone
                                    && v != DailyVerdict::TooEarly)) {
        note("report", std::string("Daily report: ") + nameOf(v, names::DAILY)
                       + (v == DailyVerdict::Sent ? "" : " (logged; retried on the next tick)"));
    }
    lastDaily = v;
}

DailyVerdict GuiWorld::runDaily() {
    DailyVerdict v = reports.generateDaily(clock.now());
    note("report", std::string("Daily report requested: ") + nameOf(v, names::DAILY));
    lastDaily = v;
    return v;
}

void GuiWorld::setHistoryFailing(bool failing) {
    historyFailing = failing;
    history.failWrites(failing);
    history.failReads(failing);
}

void GuiWorld::setSenderFailing(bool failing) {
    senderFailing = failing;
    sender.failSends(failing);
}

void GuiWorld::note(const std::string& kind, const std::string& text) {
    feed.push_back({clock.now(), kind, text});
    if (feed.size() > FEED_LIMIT) feed.erase(feed.begin());
}

void GuiWorld::noteStatusChanges() {
    for (const AdcId& adc : registry.activeAdcs()) {
        AdcStatus status = heartbeats.status(adc);
        if (shownStatus[adc] == status) continue;
        shownStatus[adc] = status;
        bool inMaintenance = maintenance.isInMaintenance(adc, clock.now());
        note("status", adcName(adc) + " is now " + nameOf(status, names::ADC_STATUS)
                       + (inMaintenance ? " (in maintenance: no alarm)" : ""));
    }
}
