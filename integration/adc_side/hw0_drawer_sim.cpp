//HW-0 drawer controller, simulated faults (build-time replacement)

/*
Replaces Subgroup_A/src/HW0_DrawerController.cpp in the integrated ADC build
only (integration/makefile), like hw1_transceiver_net.cpp replaces HW-1.
Subgroup A's own build and file are unchanged.

Same interface (Subgroup_A/inc/HW0_DrawerController.hpp). In "normal" mode it
prints the same lines and returns the same values as Subgroup A's stub. The
mode is read on every call from a control file, so it can be changed while the
ADC runs (drawer.bat stuck | jam | normal):
  normal  as Subgroup A's stub: drawers open and are closed at once
  stuck   the next drawer opened stays open: check_drawer_status says OPEN, so
          Subgroup A's BH3::MonitorDrawerSafety times out after 30 s (TIMEOUT_FAULT,
          Fault mode, "CRITICAL FAULT: Drawer N left open." -> the HMS), and
          count_open_drawers blocks the next dispense. Back to normal frees it.
  jam     Open_Drawer fails (returns 0, the drawer stays shut). Subgroup A's BH3
          ignores that return value, so the dispense is still logged and counted
          (REPORT_NOTES.md P9): this mode shows the defect, it raises nothing itself.

Setting: ADC_DRAWER_CONTROL (default run/drawer_mode.txt); a missing file means normal.
*/

#include "HW0_DrawerController.hpp"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <mutex>
#include <set>

namespace {

enum class Mode { Normal, Stuck, Jam };

std::mutex lock;          // BH3's drawer-safety thread calls in too
std::set<int> stuckOpen;  // drawers held open by the stuck mode

Mode mode() {
    const char* path = std::getenv("ADC_DRAWER_CONTROL");
    std::ifstream in(path && *path ? path : "run/drawer_mode.txt");
    std::string word;
    in >> word;
    for (char& c : word) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (word == "stuck") return Mode::Stuck;
    if (word == "jam") return Mode::Jam;
    return Mode::Normal;
}

// Back in normal mode, a stuck drawer is freed (the technician fixed it).
Mode current() {
    Mode m = mode();
    if (m == Mode::Normal) stuckOpen.clear();
    return m;
}

}  // namespace

namespace HW0 {
    int Open_Drawer(int drawer_id) {
        std::lock_guard<std::mutex> guard(lock);
        Mode m = current();
        if (m == Mode::Jam) {
            std::cout << "[HW-0] Actuator FAULT: Drawer " << drawer_id << " did not open (jammed).\n";
            return 0;
        }
        std::cout << "[HW-0] Actuator engaged: Drawer " << drawer_id << " OPENED.\n";
        if (m == Mode::Stuck) stuckOpen.insert(drawer_id);
        return 1;
    }

    int Close_Drawer(int drawer_id) {
        std::lock_guard<std::mutex> guard(lock);
        current();
        if (drawer_id == -1) {
            std::cout << "[HW-0] FORCE LOCK ENGAGED on all drawers.\n";
        } else if (stuckOpen.count(drawer_id) > 0) {
            std::cout << "[HW-0] Actuator engaged: Drawer " << drawer_id << " did not close (stuck OPEN).\n";
            return 0;
        } else {
            std::cout << "[HW-0] Actuator engaged: Drawer " << drawer_id << " CLOSED.\n";
        }
        return 1;
    }

    int count_open_drawers() {
        std::lock_guard<std::mutex> guard(lock);
        current();
        return static_cast<int>(stuckOpen.size());
    }

    std::string check_drawer_status(int drawer_id) {
        std::lock_guard<std::mutex> guard(lock);
        current();
        return stuckOpen.count(drawer_id) > 0 ? "OPEN" : "CLOSED";
    }
}
