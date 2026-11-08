//ADC output table

#include "adc_output_table.hpp"

#include <regex>
#include <vector>

namespace adc_output {

namespace {

// What a row turns its match into.
enum class Kind { Withdrawal, Deposit, Alarm };

struct Row {
    const char* id;        // named in the tests and in REPORT_NOTES.md
    std::regex pattern;    // must match the end of the line (see translate)
    Kind kind;
    const char* code;      // alarm code sent to the HMS (alarms only)
};

// Where each string comes from in Subgroup A (unchanged files):
//   R1, R2  SW2::add_event + BH4::generate_Event + BH3::RequestDrug / addDrug
//   R3      HW3::alert_security, called by BH5::alarm for INVALID_LOGIN_MAX and SECURITY_BREACH
//   R4      BH3::MonitorDrawerSafety (drawer open > 30 s, TIMEOUT_FAULT)
//   R5      main.cpp option 3 (simulated hardware jam), logged through BH4
//   R6, R7  main.cpp automated test suite (TestHardwareJam, TestNetworkLoss)
// HARDWARE_JAM and TIMEOUT_FAULT have no HMS alarm type: the HMS shows them
// as Critical but cannot resolve them (decision D3 in ADC_HMS_INTERFACE.md).
const std::vector<Row>& rows() {
    static const std::string log = R"(\[SW-2 EVENT LOG SAVED\]: \[TIMESTAMP\] )";
    static const std::vector<Row> table = {
        {"R1", std::regex(log + R"((\S+) dispensed (\d+)mg of (.+))"), Kind::Withdrawal, ""},
        {"R2", std::regex(log + R"((\S+) restocked (\d+)mg of (.+))"), Kind::Deposit, ""},
        {"R3", std::regex(R"(\[HW-3\] CRITICAL: Security Relay triggered! Hospital Staff Notified!)"),
         Kind::Alarm, "UNAUTHORIZED_ACCESS"},
        {"R4", std::regex(log + R"(CRITICAL FAULT: (Drawer \d+ left open)\.)"), Kind::Alarm, "TIMEOUT_FAULT"},
        {"R5", std::regex(log + R"(((\S+) attempted to dispense \d+mg of .+) but caused a HARDWARE JAM)"),
         Kind::Alarm, "HARDWARE_JAM"},
        {"R6", std::regex(R"(Simulating physical obstruction in drawer\.\.\.)"), Kind::Alarm, "HARDWARE_JAM"},
        {"R7", std::regex(R"(Simulating HMS connection drop\.\.\.)"), Kind::Alarm, "CONNECTION_LOSS"},
    };
    return table;
}

std::string describe(const Row& row, const std::smatch& m) {
    std::string id = row.id;
    if (id == "R3") return "ADC security lockout (failed logins or breach); security relay triggered";
    if (id == "R4") return m[1].str() + " (drawer timeout; ADC in fault mode)";
    if (id == "R5") return m[1].str() + " (drawer actuator jammed; ADC in fault mode)";
    if (id == "R6") return "drawer obstruction (ADC self-test); ADC in fault mode";
    return "ADC self-test simulated an HMS connection drop";
}

}  // namespace

std::string field(const std::string& text) {
    std::string out = text;
    for (char& c : out) {
        if (c == '|') c = '/';
    }
    return out;
}

static const std::string STAMP = "|ADC_TIME=";

std::string stamp(const std::string& message, const std::string& timeText) {
    return message + STAMP + field(timeText);
}

std::string stampOf(const std::string& message) {
    std::size_t at = message.rfind(STAMP);
    return at == std::string::npos ? "" : message.substr(at + STAMP.size());
}

std::optional<std::string> translate(int adcId, const std::string& raw) {
    std::string line = raw;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const std::string prefix = std::to_string(adcId) + "|";
    for (const Row& row : rows()) {
        // Searched for, not matched from the start: Subgroup A prints prompts
        // without a newline ("Amount to Add: "), so its log line can follow one
        // on the same console line.
        std::smatch m;
        if (!std::regex_search(line, m, row.pattern) || m.suffix().length() != 0) continue;
        switch (row.kind) {
            case Kind::Withdrawal:
            case Kind::Deposit:
                return prefix + (row.kind == Kind::Withdrawal ? "Withdrawal|" : "Deposit|") + field(m[3].str())
                       + "|" + m[2].str() + "|" + field(m[1].str());
            case Kind::Alarm:
                return prefix + "Alarm|" + row.code + "|" + field(describe(row, m));
        }
    }
    return std::nullopt;
}

}  // namespace adc_output
