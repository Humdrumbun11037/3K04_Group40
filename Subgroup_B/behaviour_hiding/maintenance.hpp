//maintenance

/*
Hides: how maintenance windows are stored, their lifecycle
       (Scheduled -> Active -> Completed, or Cancelled), and the rules for
       accepting a new window (FR-HLT-01, FR-HLT-02)

Interface
takes: requests to schedule or cancel a window for an ADC
sends: whether an ADC is in maintenance at a given time; a "Maintenance"
       history entry whenever a window is scheduled, starts, completes or is cancelled
uses: clock, ADC registry, history, permissions

Invariant: no two open (Scheduled or Active) windows for one ADC overlap.
Completed and Cancelled windows do not count.
Windows are half-open [start, end), so one window may start exactly when the
previous one ends. A window lasts at most 12 hours (exactly 12 is allowed). A window may start in the past as long as it ends in the future.
History entries never block a change: a failed write is ignored.
*/
#pragma once

#include <string>
#include <vector>

#include "behaviour_hiding/interfaces/adc_id.hpp"
#include "behaviour_hiding/interfaces/adc_registry.hpp"
#include "behaviour_hiding/interfaces/clock.hpp"
#include "behaviour_hiding/interfaces/history.hpp"
#include "behaviour_hiding/interfaces/maintenance_status.hpp"
#include "behaviour_hiding/interfaces/permissions.hpp"
#include "behaviour_hiding/interfaces/user.hpp"

enum class ScheduleVerdict { Created, NotAuthorized, UnknownAdc, InvalidWindow, WindowTooLong,
                             Overlap };
enum class CancelVerdict { Cancelled, NotAuthorized, UnknownWindow, NotCancellable };

class Maintenance : public hms::behaviour::MaintenanceStatus {
public:
    // { Post: no windows, so the invariant holds }
    Maintenance(const hms::behaviour::Clock& clock, const hms::behaviour::AdcRegistry& registry,
                hms::behaviour::History& history, const hms::behaviour::Permissions& permissions);

    // { Pre: none }
    //   schedule(user, adc, start, end)
    // { Post: the first matching row decides the result:
    //         1. permissions deny ScheduleMaintenance => NotAuthorized
    //         2. adc unregistered or decommissioned   => UnknownAdc
    //         3. end <= start, or end <= clock now    => InvalidWindow
    //         4. end - start > 12 hours               => WindowTooLong
    //         5. overlaps an open window for adc      => Overlap
    //         6. otherwise                            => Created, window added, history
    //                                                    entry "window scheduled"
    //         Rows 1-5 add no window. }
    ScheduleVerdict schedule(const hms::behaviour::User& user, const hms::behaviour::AdcId& adc,
                             long long start, long long end);

    // { Pre: none }
    //   cancel(user, adc, start)
    // { Post: the first matching row decides the result:
    //         1. permissions deny CancelMaintenance   => NotAuthorized
    //         2. adc has no window starting at start  => UnknownWindow
    //         3. that window is Completed or Cancelled => NotCancellable
    //         4. otherwise (Scheduled or Active)      => Cancelled; if it was Active,
    //                                                    suppression ends immediately;
    //                                                    history entry "window cancelled"
    //         Rows 1-3 change nothing. }
    CancelVerdict cancel(const hms::behaviour::User& user, const hms::behaviour::AdcId& adc,
                         long long start);

    // { Pre: now is not earlier than any time asked about before }
    //   isInMaintenance(adc, now)
    // { Post: every window's state is brought up to date for now, with a history
    //         entry for each start and completion, timed at the window's start or end;
    //         returns true iff adc has an Active window }
    bool isInMaintenance(const hms::behaviour::AdcId& adc, long long now) override;

private:
    enum class WindowState { Scheduled, Active, Completed, Cancelled };
    enum class WindowEvent { StartTimeReached, EndTimeReached, Cancel };

    struct Window {
        hms::behaviour::AdcId adc;
        long long start;
        long long end;
        WindowState state;
    };

    static WindowState next(WindowState state, WindowEvent event);
    static bool isOpen(WindowState state);
    void advanceTo(long long now);
    Window* findWindow(const hms::behaviour::AdcId& adc, long long start);
    void record(const std::string& who, const std::string& what, const Window& window, long long time);
    bool overlapsExisting(const hms::behaviour::AdcId& adc, long long start, long long end) const;

    const hms::behaviour::Clock& clock;
    const hms::behaviour::AdcRegistry& registry;
    hms::behaviour::History& history;
    const hms::behaviour::Permissions& permissions;
    std::vector<Window> windows;
};
