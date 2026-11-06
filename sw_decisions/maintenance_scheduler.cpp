//maintenance scheduler

/*
Hides: checking for valid maintenance schedule, tracking when maintenance schedule ends

Interface
sends: maintenance timeslot, report to history manager (report_generation)
takes: clock information
*/

#include "maintenance_scheduler.hpp"

#include <algorithm>

// --- The window ---
MaintenanceWindow::MaintenanceWindow(int id, int adcId, std::string start, std::string end,
                                     std::string scheduledBy, EventRecorder& history)
    : id(id), adcId(adcId), startTime(std::move(start)), endTime(std::move(end)),
      scheduledBy(std::move(scheduledBy)), history(history) {}

MaintenanceWindow::State MaintenanceWindow::update(const std::string& now) {
    if (state == State::Pending && now >= startTime) {
        state = State::Active;
        report("maintenance started", now);
    }
    if (state == State::Active && now >= endTime) {
        state = State::Finished;
        report("maintenance ended", now);
    }
    return state;
}

void MaintenanceWindow::report(const std::string& what, const std::string& now) {
    Event event;
    event.type = EventType::Maintenance;
    event.adcId = adcId;
    event.timestamp = now;
    event.fields["action"] = what;
    event.fields["window_id"] = std::to_string(id);
    event.fields["start"] = startTime;
    event.fields["end"] = endTime;
    event.fields["user"] = scheduledBy;
    history.record(event);
}

// --- The scheduler ---
MaintenanceScheduler::MaintenanceScheduler(const Clock& clock, EventRecorder& history)
    : clock(clock), history(history) {}

MaintenanceScheduler::Result MaintenanceScheduler::scheduleMaintenance(
        const std::string& start, const std::string& end, int adcId,
        const std::string& scheduledBy, int* newId) {
    const Result check = sanityCheck(start, end, adcId);
    if (check != Result::Ok) return check;

    const int id = nextId++;
    windows.push_back(std::make_unique<MaintenanceWindow>(id, adcId, start, end, scheduledBy, history));
    if (newId != nullptr) *newId = id;

    // Send the timeslot to the history manager
    Event event;
    event.type = EventType::Maintenance;
    event.adcId = adcId;
    event.timestamp = clock.now();
    event.fields["action"] = "maintenance scheduled";
    event.fields["window_id"] = std::to_string(id);
    event.fields["start"] = start;
    event.fields["end"] = end;
    event.fields["user"] = scheduledBy;
    history.record(event);
    return Result::Ok;
}

bool MaintenanceScheduler::cancel(int windowId) {
    auto it = std::find_if(windows.begin(), windows.end(),
                           [&](const auto& w) { return w->getId() == windowId; });
    if (it == windows.end() || (*it)->getState() != MaintenanceWindow::State::Pending) {
        return false;   // unknown, or already started
    }
    windows.erase(it);
    return true;
}

// Call this from your clock/timer loop (hourly, or better, every minute)
void MaintenanceScheduler::tick() {
    const std::string now = clock.now();
    for (auto& w : windows) w->update(now);

    // Destroy finished windows
    windows.erase(
        std::remove_if(windows.begin(), windows.end(),
            [](const std::unique_ptr<MaintenanceWindow>& w) {
                return w->getState() == MaintenanceWindow::State::Finished;
            }),
        windows.end());
}

bool MaintenanceScheduler::maintenanceActive() const {
    return std::any_of(windows.begin(), windows.end(),
        [](const auto& w) { return w->getState() == MaintenanceWindow::State::Active; });
}

bool MaintenanceScheduler::maintenanceActive(int adcId) const {
    return std::any_of(windows.begin(), windows.end(), [&](const auto& w) {
        return w->getState() == MaintenanceWindow::State::Active &&
               (w->getAdcId() == kAllAdcs || w->getAdcId() == adcId);
    });
}

std::vector<MaintenanceScheduler::Slot> MaintenanceScheduler::upcoming() const {
    std::vector<Slot> slots;
    for (const auto& w : windows) {
        slots.push_back({w->getId(), w->getAdcId(), w->getStart(), w->getEnd(),
                         w->getState() == MaintenanceWindow::State::Active});
    }
    std::sort(slots.begin(), slots.end(),
              [](const Slot& a, const Slot& b) { return a.start < b.start; });
    return slots;
}

std::string MaintenanceScheduler::describe(Result result) {
    switch (result) {
        case Result::Ok:               return "Maintenance scheduled.";
        case Result::InvalidTimestamp: return "Times must be written as YYYY-MM-DD HH:MM:SS.";
        case Result::EndBeforeStart:   return "The end time must be after the start time.";
        case Result::StartInPast:      return "Maintenance cannot be scheduled in the past.";
        case Result::TooLong:          return "Maintenance windows can be at most 12 hours long.";
        case Result::Overlaps:         return "This window overlaps maintenance already scheduled for that ADC.";
    }
    return "Incorrect dates and/or times. Please double check and reschedule.";
}

MaintenanceScheduler::Result MaintenanceScheduler::sanityCheck(
        const std::string& start, const std::string& end, int adcId) const {
    if (!timeutil::isValid(start) || !timeutil::isValid(end)) return Result::InvalidTimestamp;
    if (end <= start) return Result::EndBeforeStart;
    if (start < clock.now()) return Result::StartInPast;
    if (timeutil::secondsBetween(start, end) > kMaxWindowSeconds) return Result::TooLong;

    for (const auto& w : windows) {
        const bool sameTarget = adcId == kAllAdcs || w->getAdcId() == kAllAdcs || w->getAdcId() == adcId;
        const bool timesOverlap = start < w->getEnd() && w->getStart() < end;
        if (sameTarget && timesOverlap) return Result::Overlaps;
    }
    return Result::Ok;
}
