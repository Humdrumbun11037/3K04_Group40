//maintenance scheduler

/*
Hides: checking for valid maintenance schedule, tracking when maintenance schedule ends

Interface
sends: maintenance timeslot, report to history manager (report_generation)
takes: clock information
*/
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "external_interfaces.hpp"
#include "report_generation.hpp"

// --- The window ---
class MaintenanceWindow {
public:
    enum class State { Pending, Active, Finished };

    // adcId == kAllAdcs means the whole ADC network is under maintenance.
    MaintenanceWindow(int id, int adcId, std::string start, std::string end,
                      std::string scheduledBy, EventRecorder& history);

    State update(const std::string& now);

    State getState() const { return state; }
    int getId() const { return id; }
    int getAdcId() const { return adcId; }
    const std::string& getStart() const { return startTime; }
    const std::string& getEnd() const { return endTime; }

private:
    void report(const std::string& what, const std::string& now);

    int id;
    int adcId;
    std::string startTime, endTime, scheduledBy;
    EventRecorder& history;
    State state = State::Pending;
};

// --- The scheduler ---
class MaintenanceScheduler {
public:
    static constexpr int kAllAdcs = -1;
    // Longest window that may be booked in one go.
    static constexpr long long kMaxWindowSeconds = 12LL * 60 * 60;

    MaintenanceScheduler(const Clock& clock, EventRecorder& history);

    enum class Result { Ok, InvalidTimestamp, EndBeforeStart, StartInPast, TooLong, Overlaps };

    // Returns Ok and the new window's id on success.
    Result scheduleMaintenance(const std::string& start, const std::string& end,
                               int adcId = kAllAdcs, const std::string& scheduledBy = "",
                               int* newId = nullptr);

    // Cancels a window that has not started yet.
    bool cancel(int windowId);

    // Call this from your clock/timer loop (hourly, or better, every minute)
    void tick();

    bool maintenanceActive() const;              // any window active
    bool maintenanceActive(int adcId) const;     // a window covering this ADC is active

    struct Slot { int id; int adcId; std::string start, end; bool active; };
    std::vector<Slot> upcoming() const;          // pending and active windows, by start time

    static std::string describe(Result result);

private:
    Result sanityCheck(const std::string& start, const std::string& end, int adcId) const;

    const Clock& clock;
    EventRecorder& history;
    int nextId = 1;
    std::vector<std::unique_ptr<MaintenanceWindow>> windows;
};
