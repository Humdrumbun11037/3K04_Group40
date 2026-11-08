// Tests for the ADC output table (integration/adc_side/adc_output_table.*).
// Two layers, so any drift in Subgroup A's print strings fails here first:
//   1. Subgroup A's real modules (BH2, BH3, BH4, SW2, HW3, ...) are run and what
//      they print is fed through the table.
//   2. Strings that only main.cpp or a 30 s drawer timer can print are pinned
//      in Subgroup A's source text (read-only).
// Run from integration/ (the makefile does).

#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "BH2_LoginAuthenticator.hpp"
#include "BH3_InventoryManager.hpp"
#include "BH4_EventLogger.hpp"
#include "BH5_AlarmHandler.hpp"
#include "SystemModes.hpp"
#include "adc_side/adc_output_table.hpp"

// Subgroup A's main.cpp defines this; main.cpp is not linked into the test.
SystemState currentState = SystemState::Idle;

static int passed = 0;
static const int ADC = 3;

// Runs one piece of Subgroup A code and returns every HMS message the table
// makes from what it printed.
template <typename F>
static std::vector<std::string> messagesFrom(F run) {
    std::ostringstream captured;
    std::streambuf* original = std::cout.rdbuf(captured.rdbuf());
    run();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));   // BH3's drawer thread
    std::cout.rdbuf(original);
    std::vector<std::string> out;
    std::istringstream lines(captured.str());
    for (std::string line; std::getline(lines, line);) {
        if (auto m = adc_output::translate(ADC, line)) out.push_back(*m);
    }
    return out;
}

static std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    assert(in && "Subgroup A source not found; run from integration/");
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

static bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

// ---- Layer 1: Subgroup A's real code ----

static void r1_real_dispense_becomes_a_withdrawal() {
    currentState = SystemState::Dispensation_Mode;
    std::vector<std::string> m = messagesFrom([] { BH3::RequestDrug("Tylenol", 10, "mg", "nurse_jane"); });
    assert(m.size() == 1 && m[0] == "3|Withdrawal|Tylenol|10|nurse_jane");
    passed++;
}

static void r1_refused_dispense_sends_nothing() {
    currentState = SystemState::Dispensation_Mode;
    assert(messagesFrom([] { BH3::RequestDrug("Tylenol", 100000, "mg", "nurse_jane"); }).empty());   // insufficient stock
    assert(messagesFrom([] { BH3::RequestDrug("Aspirin", 5, "mg", "nurse_jane"); }).empty());        // not stocked
    currentState = SystemState::Fault_Mode;
    assert(messagesFrom([] { BH3::RequestDrug("Tylenol", 5, "mg", "nurse_jane"); }).empty());        // locked
    passed++;
}

static void r2_real_restock_becomes_a_deposit() {
    std::vector<std::string> m = messagesFrom([] { BH3::addDrug("Morphine", 20, "mg", 2, "pharm_bob"); });
    assert(m.size() == 1 && m[0] == "3|Deposit|Morphine|20|pharm_bob");
    passed++;
}

// As in Subgroup A's menu: its prompts have no newline, so the log line lands on
// the prompt's console line ("Drug Name: Amount to Add: [SW-2 EVENT LOG SAVED]: ...").
static void log_lines_after_a_prompt_are_still_read() {
    std::vector<std::string> m = messagesFrom([] {
        std::cout << "Drug Name: Amount to Add: ";
        BH3::addDrug("Morphine", 20, "mg", 1, "pharm_bob");
    });
    assert(m.size() == 1 && m[0] == "3|Deposit|Morphine|20|pharm_bob");
    assert(*adc_output::translate(3, "Password: [HW-3] CRITICAL: Security Relay triggered! Hospital Staff Notified!")
           == "3|Alarm|UNAUTHORIZED_ACCESS|ADC security lockout (failed logins or breach); security relay triggered");
    // Prompts alone, or a prompt then an unrelated log line, send nothing.
    assert(!adc_output::translate(3, "Drug Name: Amount to Add: "));
    assert(!adc_output::translate(3, "Dose Amount (mg): [SW-2 EVENT LOG SAVED]: [TIMESTAMP] Drawer 1 safely secured."));
    passed++;
}

static void r3_real_login_lockout_becomes_unauthorized_access() {
    currentState = SystemState::Idle;
    BH2::reset_attempts();
    std::vector<std::string> m = messagesFrom([] {
        for (int i = 0; i < 3; i++) BH2::check_user("nurse_jane", "wrong");
    });
    assert(m.size() == 1 && contains(m[0], "3|Alarm|UNAUTHORIZED_ACCESS|"));
    assert(currentState == SystemState::Security_Lockout);
    BH2::reset_attempts();
    passed++;
}

static void r3_real_security_breach_becomes_unauthorized_access() {
    std::vector<std::string> m = messagesFrom([] { BH5::alarm("SECURITY_BREACH"); });
    assert(m.size() == 1 && contains(m[0], "3|Alarm|UNAUTHORIZED_ACCESS|"));
    passed++;
}

static void other_real_alarms_print_nothing_the_table_uses() {
    // Their HMS message comes from R4-R7 (logged or printed by the caller) or,
    // for CONNECTION_LOST, from the link itself when its ping fails.
    for (const char* code : {"TIMEOUT_FAULT", "HARDWARE_JAM", "CONNECTION_LOST", "LOW_INVENTORY"}) {
        assert(messagesFrom([code] { BH5::alarm(code); }).empty());
    }
    passed++;
}

static void r4_r5_event_log_prefix_is_the_real_one() {
    std::vector<std::string> m = messagesFrom([] {
        BH4::generate_Event("CRITICAL FAULT: Drawer 2 left open.");
        BH4::generate_Event("nurse_jane attempted to dispense 5mg of Morphine but caused a HARDWARE JAM");
    });
    assert(m.size() == 2);
    assert(m[0] == "3|Alarm|TIMEOUT_FAULT|Drawer 2 left open (drawer timeout; ADC in fault mode)");
    assert(m[1] == "3|Alarm|HARDWARE_JAM|nurse_jane attempted to dispense 5mg of Morphine "
                   "(drawer actuator jammed; ADC in fault mode)");
    passed++;
}

// ---- Layer 2: strings pinned in Subgroup A's source ----

static void pinned_strings_are_still_in_subgroup_a() {
    const std::string sw2 = readFile("../Subgroup_A/src/SW2_EventDatabase.cpp");
    const std::string bh4 = readFile("../Subgroup_A/src/BH4_EventLogger.cpp");
    const std::string bh3 = readFile("../Subgroup_A/src/BH3_InventoryManager.cpp");
    const std::string hw3 = readFile("../Subgroup_A/src/HW3_AlarmExecutor.cpp");
    const std::string main = readFile("../Subgroup_A/src/main.cpp");
    assert(contains(sw2, "\"[SW-2 EVENT LOG SAVED]: \" << event_string"));
    assert(contains(bh4, "\"[TIMESTAMP] \" + event"));
    assert(contains(bh3, "username + \" dispensed \" + std::to_string(dose) + \"mg of \" + drugname"));
    assert(contains(bh3, "username + \" restocked \" + std::to_string(amount) + \"mg of \" + drugname"));
    assert(contains(bh3, "\"CRITICAL FAULT: Drawer \" + std::to_string(drawer_id) + \" left open.\""));
    assert(contains(hw3, "\"[HW-3] CRITICAL: Security Relay triggered! Hospital Staff Notified!\\n\""));
    assert(contains(main, "\" attempted to dispense \" + std::to_string(dose) + \"mg of \" + drug + \" but caused a HARDWARE JAM\""));
    assert(contains(main, "\"Simulating physical obstruction in drawer...\\n\""));
    assert(contains(main, "\"Simulating HMS connection drop...\\n\""));
    passed++;
}

// ---- The table on its own ----

static void r6_r7_self_test_lines() {
    assert(*adc_output::translate(ADC, "Simulating physical obstruction in drawer...")
           == "3|Alarm|HARDWARE_JAM|drawer obstruction (ADC self-test); ADC in fault mode");
    assert(*adc_output::translate(ADC, "Simulating HMS connection drop...")
           == "3|Alarm|CONNECTION_LOSS|ADC self-test simulated an HMS connection drop");
    passed++;
}

static void unrelated_lines_send_nothing() {
    const char* lines[] = {
        "",
        "[SW-2 EVENT LOG SAVED]: [TIMESTAMP] Drawer 1 safely secured.",
        "[SW-2 EVENT LOG SAVED]: [TIMESTAMP] admin_alice added new system user: bob (Nurse)",
        "[DISPLAY]: Dispensed. Please close drawer. Remaining Tylenol: 90",
        "[HW-1] Polling HMS connection...",
        "[HW-3] BEEP! BEEP! BEEP! (High voltage signal sent to speaker)",
        " Current System State: FAULT MODE (Hardware Locked)",
        "nurse_jane dispensed 10mg of Tylenol",   // not from the event log
    };
    for (const char* line : lines) assert(!adc_output::translate(ADC, line));
    passed++;
}

static void names_with_spaces_bars_and_crlf() {
    assert(*adc_output::translate(7, "[SW-2 EVENT LOG SAVED]: [TIMESTAMP] pharm_bob restocked 5mg of Insulin glargine\r")
           == "7|Deposit|Insulin glargine|5|pharm_bob");
    assert(*adc_output::translate(3, "[SW-2 EVENT LOG SAVED]: [TIMESTAMP] a|b dispensed 1mg of X|Y")
           == "3|Withdrawal|X/Y|1|a/b");
    passed++;
}

int main() {
    r1_real_dispense_becomes_a_withdrawal();
    r1_refused_dispense_sends_nothing();
    r2_real_restock_becomes_a_deposit();
    log_lines_after_a_prompt_are_still_read();
    r3_real_login_lockout_becomes_unauthorized_access();
    r3_real_security_breach_becomes_unauthorized_access();
    other_real_alarms_print_nothing_the_table_uses();
    r4_r5_event_log_prefix_is_the_real_one();
    pinned_strings_are_still_in_subgroup_a();
    r6_r7_self_test_lines();
    unrelated_lines_send_nothing();
    names_with_spaces_bars_and_crlf();
    std::cout << "adc_output_table_test: " << passed << " tests passed\n";
    return 0;
}
