//maintenance scheduler

/*
Hides: checking for valid maintenance schedule, tracking when maintenance schedule ends

Interface
sends: maintenance timeslot, report to history manager
takes: clock information
*/
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <cstdio>
//Import whatever I'm planning to do for report generation here
//Import the clock interface
class Clock {
public:
    virtual ~Clock() = default;
    virtual std::string now() const = 0;   // ISO 8601, e.g. "2026-10-30 14:05:09"
};

class HistoryManager {
public:
    virtual ~HistoryManager() = default;
    virtual void report(const std::string& event,
                        const std::string& start,
                        const std::string& end) = 0;
};

//Need to properly interface with the above ^^^^^
// --- The window ---
class MaintenanceWindow {
public:
    enum class State { Pending, Active, Finished };

    MaintenanceWindow(std::string start, std::string end, HistoryManager& history)
        : startTime(std::move(start)), endTime(std::move(end)), history(history) {}

    State update(const std::string& now) {
        if (state == State::Pending && now >= startTime) {
            state = State::Active;
            history.report("maintenance started", startTime, endTime);
        }
        if (state == State::Active && now >= endTime) {
            state = State::Finished;
            history.report("maintenance ended", startTime, endTime);
        }
        return state;
    }

    State getState() const { return state; }

private:
    std::string startTime, endTime;
    HistoryManager& history;
    State state = State::Pending;
};

// --- The scheduler ---
class MaintenanceScheduler {
public:
    MaintenanceScheduler(Clock& clock, HistoryManager& history)
        : clock(clock), history(history) {}

    bool scheduleMaintenance(const std::string& start, const std::string& end) {
        if (!sanityCheck(start, end)) {
            std::fprintf(stderr, "Incorrect dates and/or times. Please double check and reschedule.\n");
            return false;
        }
        windows.push_back(std::make_unique<MaintenanceWindow>(start, end, history));
        return true;
    }

    // Call this from your clock/timer loop (hourly, or better, every minute)
    void tick() {
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

    bool maintenanceActive() const {
        return std::any_of(windows.begin(), windows.end(),
            [](const auto& w) { return w->getState() == MaintenanceWindow::State::Active; });
    }

private:
    bool sanityCheck(const std::string& start, const std::string& end) const {
        return end > start;   // add `&& start >= clock.now()` if past windows should be rejected
    }

    Clock& clock;
    HistoryManager& history;
    std::vector<std::unique_ptr<MaintenanceWindow>> windows;
};