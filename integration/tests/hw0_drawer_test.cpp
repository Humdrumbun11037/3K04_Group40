// Tests for the simulated drawer hardware (integration/adc_side/hw0_drawer_sim.cpp)
// driving Subgroup A's real BH3 (dispensing and drawer safety) and BH5 (alarms).
// The stuck-drawer case waits for Subgroup A's own 30 s drawer timeout, so this
// test takes about 35 s.

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "BH3_InventoryManager.hpp"
#include "HW0_DrawerController.hpp"
#include "SW1_InventoryDatabase.hpp"
#include "SystemModes.hpp"
#include "adc_side/adc_output_table.hpp"
#include "adc_side/console_observer.hpp"

// Subgroup A's main.cpp defines this; main.cpp is not linked into the test.
SystemState currentState = SystemState::Idle;

static int passed = 0;
static const char* const CONTROL = "build/hw0_test/drawer_mode.txt";

static void setMode(const std::string& word) {
    std::ofstream(CONTROL) << word << "\r\n";   // as drawer.bat writes it
}

// Everything printed while the test runs (BH3's safety thread prints too),
// and the HMS messages the table makes from it.
static std::mutex seenLock;
static std::string printed;
static std::vector<std::string> messages;

static bool sent(const std::string& start) {
    std::lock_guard<std::mutex> guard(seenLock);
    for (const std::string& m : messages) {
        if (m.rfind(start, 0) == 0) return true;
    }
    return false;
}

static void normal_mode_is_subgroup_as_stub() {
    setMode("normal");
    assert(HW0::Open_Drawer(1) == 1 && HW0::check_drawer_status(1) == "CLOSED" && HW0::count_open_drawers() == 0);
    currentState = SystemState::Dispensation_Mode;
    assert(BH3::RequestDrug("Tylenol", 5, "mg", "nurse_jane") == 0);
    std::filesystem::remove(CONTROL);   // no control file: normal
    assert(HW0::Open_Drawer(2) == 1 && HW0::count_open_drawers() == 0);
    passed++;
}

// Subgroup A's BH3 ignores Open_Drawer's result: a jammed drawer still counts
// as a dispense. This test pins that behaviour (REPORT_NOTES.md P9).
static void jam_is_not_noticed_by_subgroup_a() {
    setMode("jam");
    assert(HW0::Open_Drawer(1) == 0);
    currentState = SystemState::Dispensation_Mode;
    int before = SW1::get_drug_amount("Tylenol");
    assert(BH3::RequestDrug("Tylenol", 5, "mg", "nurse_jane") == 0);
    assert(SW1::get_drug_amount("Tylenol") == before - 5);
    assert(sent("3|Withdrawal|Tylenol|5|nurse_jane"));
    setMode("normal");
    passed++;
}

static void stuck_drawer_times_out_into_fault_mode_and_reaches_the_hms() {
    setMode("STUCK");   // any case
    currentState = SystemState::Dispensation_Mode;
    assert(BH3::RequestDrug("Morphine", 5, "mg", "nurse_jane") == 0);   // Morphine is drawer 2
    assert(HW0::check_drawer_status(2) == "OPEN" && HW0::count_open_drawers() == 1);
    assert(BH3::RequestDrug("Tylenol", 5, "mg", "nurse_jane") == -3);   // "Close open drawer first"
    assert(HW0::Close_Drawer(2) == 0);                                  // won't close
    for (int s = 0; s < 40 && currentState != SystemState::Fault_Mode; s++) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    assert(currentState == SystemState::Fault_Mode);   // Subgroup A's 30 s drawer timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    assert(sent("3|Alarm|TIMEOUT_FAULT|Drawer 2 left open"));
    {
        std::lock_guard<std::mutex> guard(seenLock);
        assert(printed.find("[HW-0] Actuator engaged: Drawer 2 did not close (stuck OPEN).") != std::string::npos);
        assert(printed.find("SYSTEM LOCKED: DRAWER TIMEOUT") != std::string::npos);
    }
    setMode("normal");   // the technician frees it
    assert(HW0::check_drawer_status(2) == "CLOSED" && HW0::count_open_drawers() == 0);
    passed++;
}

int main() {
    std::filesystem::create_directories("build/hw0_test");
#ifdef _WIN32
    _putenv((std::string("ADC_DRAWER_CONTROL=") + CONTROL).c_str());
#else
    setenv("ADC_DRAWER_CONTROL", CONTROL, 1);
#endif
    std::ostringstream sink;
    std::streambuf* console = std::cout.rdbuf();
    ConsoleObserver observer(sink.rdbuf(), [](const std::string& line) {
        std::lock_guard<std::mutex> guard(seenLock);
        printed += line + "\n";
        if (auto m = adc_output::translate(3, line)) messages.push_back(*m);
    });
    std::cout.rdbuf(&observer);

    normal_mode_is_subgroup_as_stub();
    jam_is_not_noticed_by_subgroup_a();
    stuck_drawer_times_out_into_fault_mode_and_reaches_the_hms();

    std::cout.rdbuf(console);
    std::cout << "hw0_drawer_test: " << passed << " tests passed\n";
    return 0;
}
