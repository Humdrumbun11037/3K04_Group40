// Tests for the Ping fix (Subgroup_A/src/BH0_HMSComms.cpp, BH3_InventoryManager.cpp)
// with the networked transceiver (integration/adc_side/hw1_transceiver_net.cpp):
// Subgroup A's real BH0::verify_connection (menu option 5) and BH3::RequestDrug,
// against a stand-in HMS that can be up or cut.
//   Ping succeeds  -> mode unchanged, dispensing works
//   Ping fails     -> Offline, CONNECTION_LOST on the ADC, CONNECTION_LOSS queued for the HMS,
//                     local dispensing still works (queued)
//   Link restored  -> Ping succeeds -> back to Dispensation; queued messages arrive in order

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "BH0_HMSComms.hpp"
#include "BH3_InventoryManager.hpp"
#include "SystemModes.hpp"
#include "adc_side/adc_link.hpp"
#include "adc_side/hw1_net.hpp"
#include "tests/fake_hms.hpp"

// Subgroup A's main.cpp defines this; main.cpp is not linked into the test.
SystemState currentState = SystemState::Idle;

static int passed = 0;
static const int PORT = 19042;

static void setEnv(const char* name, const std::string& value) {
#ifdef _WIN32
    _putenv((std::string(name) + "=" + value).c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

static bool hasEvent(FakeHms& hms, const std::string& start) {
    for (const std::string& m : hms.events()) {
        if (m.rfind(start, 0) == 0) return true;
    }
    return false;
}

static void ping_success_keeps_dispensing(FakeHms& hms) {
    currentState = SystemState::Dispensation_Mode;
    BH0::verify_connection();
    assert(currentState == SystemState::Dispensation_Mode);   // was Offline before the fix
    assert(BH3::RequestDrug("Tylenol", 10, "mg", "nurse_jane") == 0);
    assert(within(3000, [&] { return hasEvent(hms, "3|Withdrawal|Tylenol|10|nurse_jane|ADC_TIME="); }));
    passed++;
}

static void ping_from_idle_leaves_idle() {
    currentState = SystemState::Idle;
    BH0::verify_connection();
    assert(currentState == SystemState::Idle);
    passed++;
}

static void ping_failure_goes_offline_and_queues_connection_loss(FakeHms& hms) {
    currentState = SystemState::Dispensation_Mode;
    hms.cut = true;
    std::size_t before = hms.events().size();
    BH0::verify_connection();
    assert(currentState == SystemState::Offline_Mode);
    assert(hw1_net::link()->waiting() >= 1);   // CONNECTION_LOSS, waiting for the link
    // Local dispensing continues while offline (the BH3 half of the fix), queued too.
    assert(BH3::RequestDrug("Morphine", 5, "mg", "nurse_jane") == 0);
    assert(hw1_net::link()->waiting() >= 2);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    assert(hms.events().size() == before);   // nothing got through
    passed++;
}

static void link_restored_dispensing_recovers(FakeHms& hms) {
    hms.cut = false;
    BH0::verify_connection();   // the user pings again
    assert(currentState == SystemState::Dispensation_Mode);
    assert(within(5000, [&] { return hw1_net::link()->waiting() == 0; }));
    std::vector<std::string> e = hms.events();
    assert(e.size() >= 3);
    assert(e[e.size() - 2].rfind("3|Alarm|CONNECTION_LOSS|", 0) == 0);   // in the order they happened
    assert(e[e.size() - 1].rfind("3|Withdrawal|Morphine|5|nurse_jane|ADC_TIME=", 0) == 0);
    assert(BH3::RequestDrug("Tylenol", 1, "mg", "nurse_jane") == 0);
    assert(within(3000, [&] { return hasEvent(hms, "3|Withdrawal|Tylenol|1|nurse_jane"); }));
    passed++;
}

static void ping_in_fault_mode_changes_nothing() {
    currentState = SystemState::Fault_Mode;
    BH0::verify_connection();
    assert(currentState == SystemState::Fault_Mode);
    assert(BH3::RequestDrug("Tylenol", 1, "mg", "nurse_jane") == -1);   // still locked
    passed++;
}

int main() {
    FakeHms hms(PORT);
    std::filesystem::create_directories("build/ping_test");
    std::ofstream("build/ping_test/key.bin", std::ios::binary)
        .write(reinterpret_cast<const char*>(hms.key.data()), static_cast<std::streamsize>(hms.key.size()));
    setEnv("HMS_KEY", "build/ping_test/key.bin");
    setEnv("HMS_PORT", std::to_string(PORT));
    setEnv("ADC_HEARTBEAT_SECONDS", "1");
    setEnv("ADC_ID", "3");   // no selection screen; the stand-in HMS has no cabinet list, so Subgroup A's own stock is kept
    hms.start();
    hw1_net::start();
    assert(hw1_net::link() && within(3000, [] { return hw1_net::link()->up(); }));

    ping_success_keeps_dispensing(hms);
    ping_from_idle_leaves_idle();
    ping_failure_goes_offline_and_queues_connection_loss(hms);
    link_restored_dispensing_recovers(hms);
    ping_in_fault_mode_changes_nothing();
    std::cout << "adc_ping_test: " << passed << " tests passed\n";
    return 0;
}
