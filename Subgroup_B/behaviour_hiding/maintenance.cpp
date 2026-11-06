//maintenance

#include "behaviour_hiding/maintenance.hpp"

using namespace hms::behaviour;

// The longest a single window may last. Changing it means changing only this number.
static const long long MAX_WINDOW_SECONDS = 12 * 60 * 60;

Maintenance::Maintenance(const Clock& clock, const AdcRegistry& registry, History& history,
                         const Permissions& permissions)
    : clock(clock), registry(registry), history(history), permissions(permissions) {}

// Rows are checked in table order, so the first matching row wins.
// This is what makes the table disjoint.
ScheduleVerdict Maintenance::schedule(const User& user, const AdcId& adc,
                                      long long start, long long end) {
    advanceTo(clock.now());   // so finished windows no longer count as open
    if (!permissions.mayPerform(user.role, Action::ScheduleMaintenance))
                                                return ScheduleVerdict::NotAuthorized; // row 1
    if (!registry.isActive(adc))                return ScheduleVerdict::UnknownAdc;    // row 2
    if (end <= start || end <= clock.now())     return ScheduleVerdict::InvalidWindow; // row 3
    if (end - start > MAX_WINDOW_SECONDS)       return ScheduleVerdict::WindowTooLong; // row 4
    if (overlapsExisting(adc, start, end))      return ScheduleVerdict::Overlap;       // row 5
    windows.push_back({adc, start, end, WindowState::Scheduled});                      // row 6
    record(user.name, "scheduled", windows.back(), clock.now());
    return ScheduleVerdict::Created;
}

// Rows are checked in order, like schedule. Window states are brought up to date
// first, so a window that has already ended reads as finished, not cancellable.
CancelVerdict Maintenance::cancel(const User& user, const AdcId& adc, long long start) {
    advanceTo(clock.now());   // so a window that has ended reads as Completed
    Window* window = findWindow(adc, start);
    if (!permissions.mayPerform(user.role, Action::CancelMaintenance))
                                                return CancelVerdict::NotAuthorized;   // row 1
    if (window == nullptr)                      return CancelVerdict::UnknownWindow;   // row 2
    if (!isOpen(window->state))                 return CancelVerdict::NotCancellable;  // row 3
    window->state = next(window->state, WindowEvent::Cancel);                          // row 4
    record(user.name, "cancelled", *window, clock.now());
    return CancelVerdict::Cancelled;
}

// Catches up on any windows that have started or ended, then looks for an active one.
bool Maintenance::isInMaintenance(const AdcId& adc, long long now) {
    advanceTo(now);
    for (const Window& w : windows) {
        if (w.adc == adc && w.state == WindowState::Active) return true;
    }
    return false;
}

// Window lifecycle as a transition table: one row per allowed transition.
// Any (state, event) pair not listed is invalid and leaves the state unchanged.
Maintenance::WindowState Maintenance::next(WindowState state, WindowEvent event) {
    struct Transition { WindowState from; WindowEvent event; WindowState to; };
    static const Transition table[] = {
        {WindowState::Scheduled, WindowEvent::StartTimeReached, WindowState::Active},    // T1
        {WindowState::Active,    WindowEvent::EndTimeReached,   WindowState::Completed}, // T2
        {WindowState::Scheduled, WindowEvent::Cancel,           WindowState::Cancelled}, // T3
        {WindowState::Active,    WindowEvent::Cancel,           WindowState::Cancelled}, // T4
    };
    for (const Transition& t : table) {
        if (t.from == state && t.event == event) return t.to;
    }
    return state;
}

// Open windows are the ones that can still suppress alarms or be cancelled.
bool Maintenance::isOpen(WindowState state) {
    return state == WindowState::Scheduled || state == WindowState::Active;
}

// Feed each window the time events that have happened by now, recording each
// actual change at the moment it happened (the window's start or end).
// Start is fed before end, so a window whose whole span has passed goes
// Scheduled -> Active -> Completed in one call.
void Maintenance::advanceTo(long long now) {
    for (Window& w : windows) {
        if (now >= w.start) {
            WindowState before = w.state;
            w.state = next(w.state, WindowEvent::StartTimeReached);
            if (w.state != before) record("HMS", "started", w, w.start);
        }
        if (now >= w.end) {
            WindowState before = w.state;
            w.state = next(w.state, WindowEvent::EndTimeReached);
            if (w.state != before) record("HMS", "completed", w, w.end);
        }
    }
}

// A failed write is ignored on purpose: the history must never block a change.
void Maintenance::record(const std::string& who, const std::string& what, const Window& window,
                         long long time) {
    std::string detail = adcName(window.adc) + " window " + what + " (start=" + std::to_string(window.start)
                       + " end=" + std::to_string(window.end) + ")";
    history.record({"Maintenance", who, detail, time, window.adc, ""});
}

// The window for adc starting at start. Old closed windows may share a start
// with a newer open one, so the open one is preferred; at most one open window
// can match, because open windows never overlap.
Maintenance::Window* Maintenance::findWindow(const AdcId& adc, long long start) {
    Window* closedMatch = nullptr;
    for (Window& w : windows) {
        if (w.adc != adc || w.start != start) continue;
        if (isOpen(w.state)) return &w;
        closedMatch = &w;
    }
    return closedMatch;
}

// Two half-open windows [a, b) and [c, d) overlap iff a < d and c < b.
bool Maintenance::overlapsExisting(const AdcId& adc, long long start, long long end) const {
    for (const Window& w : windows) {
        if (w.adc == adc && isOpen(w.state) && start < w.end && w.start < end) {
            return true;
        }
    }
    return false;
}
