// HMS behaviour-hiding demo: a console menu that drives the real behaviour
// modules, with fakes standing in for teammates' modules (clock, history,
// ADC registry, heartbeat counter, report sender, alarm notification).
// All input and output happens in this file; the modules never read or print.

#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "behaviour_hiding/adc_heartbeats.hpp"
#include "behaviour_hiding/alarm_resolution.hpp"
#include "behaviour_hiding/hospital_stock.hpp"
#include "behaviour_hiding/inventory_record.hpp"
#include "behaviour_hiding/maintenance.hpp"
#include "behaviour_hiding/report_formatting.hpp"
#include "behaviour_hiding/report_types.hpp"
#include "tests/fakes/fake_adc_registry.hpp"
#include "tests/fakes/fake_clock.hpp"
#include "tests/fakes/fake_heartbeat_counter.hpp"
#include "tests/fakes/fake_permissions.hpp"
#include "tests/fakes/fake_history.hpp"
#include "tests/fakes/fake_report_sender.hpp"

using namespace hms::behaviour;

static const long long START_TIME = 1793598000;   // 2026-11-02 05:40:00 local
static const long long TICK_SECONDS = 10;          // periodic checks run this often
static const long long HEARTBEAT_SECONDS = 60;     // simulated ADCs beat this often
static const long long DAY_SECONDS = 24 * 60 * 60;

// ---- Names for printing. Each list is in the same order as its enum. ----

template <typename E, std::size_t N>
static const char* nameOf(E value, const char* const (&names)[N]) {
    std::size_t i = static_cast<std::size_t>(value);
    return i < N ? names[i] : "?";
}
static const char* const SEVERITY[] = {"Low", "High", "Critical"};
static const char* const ALARM_TYPE[] = {"UnauthorizedAccess", "AdcOffline", "IncorrectMessageFormat",
                                         "InventoryLow", "ConnectionLoss", "HospitalLowStock",
                                         "InventoryDiscrepancy"};
static const char* const ROLE[] = {"Pharmacist", "Technician", "Administrator"};
static const char* const ALARM_STATE[] = {"Raised", "Acknowledged", "Resolved"};
static const char* const ADC_STATUS[] = {"Online", "Offline", "NotMonitored"};
static const char* const HOSPITAL[] = {"NotLow", "Low", "UnknownMedication"};
static const char* const RESERVE[] = {"Updated", "NotAuthorized", "UnknownMedication", "IncorrectValue",
                                      "NoExplanation", "StorageFailure"};
static const char* const ADD[] = {"Added", "JoinedOpenAlarm", "UnknownType"};
static const char* const ACK[] = {"Acknowledged", "NotAuthorized", "UnknownAlarm", "NotRaised",
                                  "StorageFailure"};
static const char* const RESOLVE[] = {"Resolved", "NotAuthorized", "UnknownAlarm", "AlreadyResolved",
                                      "NoExplanation", "StorageFailure"};
static const char* const SCHEDULE[] = {"Created", "NotAuthorized", "UnknownAdc", "InvalidWindow",
                                       "WindowTooLong", "Overlap"};
static const char* const CANCEL[] = {"Cancelled", "NotAuthorized", "UnknownWindow", "NotCancellable"};
static const char* const INVENTORY[] = {"Updated", "NotAuthorized", "UnknownItem", "IncorrectValue",
                                        "NoExplanation", "StorageFailure", "DiscrepancyRecorded"};
static const char* const DAILY[] = {"Sent", "AlreadyDone", "TooEarly", "GenerationFailed", "SendFailed"};
static const char* const SUMMARY[] = {"Summarized", "NotAuthorized", "InvalidRange", "ReadFailure",
                                      "AuditFailure"};

// ---- Stand-in for alarm notification (Inodh's module) ----

// Hands every raised alarm to alarm resolution and prints it. Choosing the
// destination role per alarm type is notification's job; this is a demo guess.
class DemoNotifier : public AlarmNotifier {
public:
    DemoNotifier(AlarmResolution& alarms, const Clock& clock, const ReportFormatting& format)
        : alarms(alarms), clock(clock), format(format) {}

    void raise(const AdcId& adc, AlarmType type, const std::string& detail = "") override {
        AddResult r = alarms.add(adc, type, destinationFor(type), detail);
        std::cout << "[" << format.formatTime(clock.now()) << "] ALARM " << nameOf(type, ALARM_TYPE)
                  << (detail.empty() ? "" : " (" + detail + ")")
                  << " for " << adcName(adc) << ": " << nameOf(r.verdict, ADD);
        if (r.verdict != AddVerdict::UnknownType) {
            AlarmInfo info = *alarms.find(r.id);
            std::cout << " #" << r.id << " (" << nameOf(info.severity, SEVERITY) << ", to "
                      << nameOf(info.destination, ROLE) << ")";
        }
        std::cout << "\n";
    }

private:
    static Role destinationFor(AlarmType type) {
        switch (type) {
            case AlarmType::InventoryLow:         return Role::Pharmacist;
            case AlarmType::HospitalLowStock:     return Role::Pharmacist;
            case AlarmType::InventoryDiscrepancy: return Role::Pharmacist;
            case AlarmType::UnauthorizedAccess:   return Role::Administrator;
            default:                              return Role::Technician;
        }
    }

    AlarmResolution& alarms;
    const Clock& clock;
    const ReportFormatting& format;
};

// ---- The demo world: every module and fake, wired together ----

// Everything the demo needs in one place: the real behaviour modules wired to
// fakes. Starting a scenario builds a fresh World, which is how the demo restarts.
struct World {
    FakeClock clock{START_TIME};
    FakeHistory history;
    FakeAdcRegistry registry;
    FakeHeartbeatCounter heartbeatLog;
    FakeReportSender sender;
    ReportFormatting format;
    FakePermissions permissions;            // the team's agreed table
    AlarmResolution alarms{clock, history, permissions};
    DemoNotifier notifier{alarms, clock, format};
    Maintenance maintenance{clock, registry, history, permissions};
    AdcHeartbeats heartbeats{clock, maintenance, notifier, history};
    InventoryRecord inventory{clock, history, notifier, permissions};
    HospitalStock hospital{clock, inventory, history, notifier, permissions};
    ReportTypes reports{clock, history, history, heartbeatLog, registry, sender, permissions};

    User user{"Tara", Role::Technician};
    std::set<AdcId> silenced;               // simulated ADCs that stopped sending heartbeats
    std::map<AdcId, AdcStatus> shownStatus; // last status printed, to report changes
    DailyVerdict lastDaily = DailyVerdict::TooEarly;
    bool historyFailing = false;
    bool senderFailing = false;

    World() {
        addAdc(1);
        addAdc(2);
        // Pretend both ADCs have been running for a day, one heartbeat a minute up
        // to and including the start time, so the daily report flags only ADCs
        // that go silent during the demo.
        for (const AdcId& adc : registry.activeAdcs()) {
            for (long long t = START_TIME - DAY_SECONDS + HEARTBEAT_SECONDS; t <= START_TIME;
                 t += HEARTBEAT_SECONDS) {
                heartbeatLog.record(adc, t);
            }
        }
        inventory.addItem(1, "Morphine", 40, 100);
        inventory.addItem(1, "Insulin", 30, 100);
        inventory.addItem(2, "Morphine", 60, 100);
        // Pharmacy reserves (made-up levels). Insulin starts just above 25% across
        // the hospital (160 of 600); `reserve Insulin 20 ...` makes it low while
        // ADC-1 (30%) stays fine. Heparin is held only in reserve.
        hospital.addReserve("Morphine", 300, 400);
        hospital.addReserve("Insulin", 130, 500);
        hospital.addReserve("Heparin", 150, 200);
    }

    void addAdc(const AdcId& adc) {
        registry.add(adc);
        heartbeats.monitor(adc);
        shownStatus[adc] = AdcStatus::Online;
    }

    std::string now() const { return "[" + format.formatTime(clock.now()) + "]"; }
};

// Delivers one heartbeat from an ADC, and logs it so the daily report can count it.
static void sendHeartbeat(World& w, const AdcId& adc) {
    if (w.heartbeats.heartbeatReceived(adc) == HeartbeatVerdict::Accepted) {
        w.heartbeatLog.record(adc, w.clock.now());
    }
}

// Status changes since last shown, plus maintenance context.
static void showStatusChanges(World& w) {
    for (const AdcId& adc : w.registry.activeAdcs()) {
        AdcStatus status = w.heartbeats.status(adc);
        if (w.shownStatus[adc] == status) continue;
        w.shownStatus[adc] = status;
        std::cout << w.now() << " " << adcName(adc) << " is now " << nameOf(status, ADC_STATUS);
        if (w.maintenance.isInMaintenance(adc, w.clock.now())) std::cout << " (in maintenance: no alarm)";
        std::cout << "\n";
    }
}

// Gives the daily report its chance to run, printing only results worth seeing.
static void runDaily(World& w, bool always) {
    DailyVerdict v = w.reports.generateDaily(w.clock.now());
    if (v == DailyVerdict::Sent) {
        std::cout << w.now() << " Daily report sent:\n" << w.format.formatDaily(w.sender.sent.back());
    } else if (always || (v != w.lastDaily && v != DailyVerdict::AlreadyDone && v != DailyVerdict::TooEarly)) {
        std::cout << w.now() << " Daily report: " << nameOf(v, DAILY);
        if (v == DailyVerdict::GenerationFailed || v == DailyVerdict::SendFailed) {
            std::cout << " (logged; retried on the next tick)";
        }
        std::cout << "\n";
    }
    w.lastDaily = v;
}

// Hospital-wide low stock: an alarm only when a medication turns low.
static void checkHospital(World& w) {
    w.hospital.checkAll();
}

// The periodic checks the HMS main loop would run every tick.
static void periodicChecks(World& w) {
    w.heartbeats.checkAll();
    checkHospital(w);
    showStatusChanges(w);
    for (const Escalation& e : w.alarms.escalateDue()) {
        std::cout << w.now() << " ESCALATED alarm #" << e.id << ": now to "
                  << nameOf(e.destination, ROLE) << ", " << nameOf(e.severity, SEVERITY) << "\n";
    }
    runDaily(w, false);
}

// Moves the fake clock forward in 10 s steps, sending heartbeats from ADCs that
// are not silenced and running the periodic checks at each step.
static void advance(World& w, long long seconds) {
    while (seconds > 0) {
        long long step = seconds < TICK_SECONDS ? seconds : TICK_SECONDS;
        long long minuteBefore = w.clock.now() / HEARTBEAT_SECONDS;
        w.clock.advance(step);
        seconds -= step;
        if (w.clock.now() / HEARTBEAT_SECONDS != minuteBefore) {
            for (const AdcId& adc : w.registry.activeAdcs()) {
                if (w.silenced.count(adc) == 0) sendHeartbeat(w, adc);
            }
        }
        periodicChecks(w);
    }
    std::cout << "  clock is now " << w.format.formatTime(w.clock.now()) << "\n";
}

// The latest HospitalLowStock alarm for a medication: "#id State", or "none".
static std::string hospitalAlarm(World& w, const std::string& medication) {
    std::string latest = "none";
    for (AlarmId id = 1; w.alarms.find(id).has_value(); id++) {
        AlarmInfo a = *w.alarms.find(id);
        if (a.adc == HospitalStock::HOSPITAL && a.type == AlarmType::HospitalLowStock
            && a.detail == medication) {
            latest = "#" + std::to_string(id) + " " + nameOf(a.state, ALARM_STATE);
        }
    }
    return latest;
}

// Prints every alarm with its type, ADC, severity, state and who it goes to.
static void showAlarms(World& w) {
    // Alarm ids are handed out 1, 2, 3, ... so this walks them all.
    int shown = 0;
    for (AlarmId id = 1; w.alarms.find(id).has_value(); id++, shown++) {
        AlarmInfo a = *w.alarms.find(id);
        std::cout << "  #" << id << " " << nameOf(a.type, ALARM_TYPE)
                  << (a.detail.empty() ? "" : " (" + a.detail + ")") << " " << adcName(a.adc) << " "
                  << nameOf(a.severity, SEVERITY) << " " << nameOf(a.state, ALARM_STATE)
                  << " to " << nameOf(a.destination, ROLE) << (a.escalated ? " (escalated)" : "")
                  << " raised " << w.format.formatTime(a.raisedAt) << "\n";
    }
    if (shown == 0) std::cout << "  (no alarms)\n";
}

// "ADC-3" or "3" gives 3; anything else gives 0, which is never a real ADC.
static AdcId parseAdc(const std::string& text) {
    std::string digits = text.rfind("ADC-", 0) == 0 ? text.substr(4) : text;
    if (digits.empty() || digits.size() > 9) return 0;
    for (char c : digits) {
        if (c < '0' || c > '9') return 0;
    }
    return std::stoi(digits);
}

static AdcId readAdc(std::istringstream& in) {
    std::string word;
    in >> word;
    return parseAdc(word);
}

// Switches the demo user; each role has a fixed demo name.
static bool parseRole(const std::string& text, User& user) {
    if (text == "technician")    { user = {"Tara", Role::Technician};     return true; }
    if (text == "pharmacist")    { user = {"Priya", Role::Pharmacist};    return true; }
    if (text == "administrator") { user = {"Alex", Role::Administrator};  return true; }
    return false;
}

// Maps the words typed after `alarm` to alarm types.
static bool parseAlarmType(const std::string& text, AlarmType& type) {
    if (text == "offline")      { type = AlarmType::AdcOffline;             return true; }
    if (text == "connection")   { type = AlarmType::ConnectionLoss;         return true; }
    if (text == "format")       { type = AlarmType::IncorrectMessageFormat; return true; }
    if (text == "lowstock")     { type = AlarmType::InventoryLow;           return true; }
    if (text == "unauthorized") { type = AlarmType::UnauthorizedAccess;     return true; }
    return false;
}

// The rest of the line after the words already read, without leading spaces.
static std::string restOfLine(std::istringstream& in) {
    std::string rest;
    std::getline(in, rest);
    std::size_t start = rest.find_first_not_of(' ');
    return start == std::string::npos ? "" : rest.substr(start);
}

// The command list shown at start-up and by `help`.
static void help() {
    std::cout <<
        "Commands (times in seconds; FROM/TO are offsets from now, negative = past):\n"
        "  time                          show the clock and current user\n"
        "  advance N                     move the clock N seconds, running the periodic checks\n"
        "                                (heartbeats, escalation, daily report) every 10 s\n"
        "  heartbeat ADC                 send one heartbeat from ADC\n"
        "  silence ADC | resume ADC      stop / restart ADC's automatic heartbeats (one per minute)\n"
        "  add-adc ADC | decommission ADC   (ADC is written ADC-3 or just 3)\n"
        "  status                        show every ADC's status\n"
        "  user technician|pharmacist|administrator\n"
        "  schedule ADC FROM TO          maintenance window, as the current user\n"
        "  cancel ADC START              cancel the window starting at START (as printed by schedule)\n"
        "  alarm ADC TYPE [DETAIL]       raise an alarm: offline|connection|format|lowstock|unauthorized\n"
        "  alarms                        list alarms\n"
        "  ack ID | resolve ID NOTE      acknowledge / resolve an alarm, as the current user\n"
        "  stock                         list inventory\n"
        "  hospital [MED]                stock across the hospital (reserves + every ADC) and its alarm\n"
        "  reserve MED N EXPLANATION     manual change to a pharmacy reserve, as the current user\n"
        "  withdraw ADC MED N            an ADC upload withdrawing N (more than the count: discrepancy)\n"
        "  deposit ADC MED N             an ADC upload depositing N\n"
        "  set ADC MED N EXPLANATION     manual inventory change, as the current user\n"
        "  report                        try the daily report now\n"
        "  summary FROM TO [adc=X] [med=Y] [actor=Z]   activity summary, as the current user\n"
        "  history-fail | sender-fail    toggle history (read and write) / report sending failure\n"
        "  scenario offline | scenario maintenance     reset and run a scripted scenario\n"
        "  help | quit\n";
}

// Runs one command line. Returns false when the user quits.
static bool run(World& w, const std::string& line) {
    std::istringstream in(line);
    std::string cmd;
    in >> cmd;
    if (cmd.empty() || cmd[0] == '#') return true;

    if (cmd == "quit") return false;
    if (cmd == "help") { help(); return true; }

    if (cmd == "time") {
        std::cout << "  " << w.format.formatTime(w.clock.now()) << " (" << w.clock.now() << " s), user "
                  << w.user.name << " (" << nameOf(w.user.role, ROLE) << ")\n";
    } else if (cmd == "advance") {
        long long n = 0;
        if (in >> n && n > 0) advance(w, n); else std::cout << "  usage: advance N\n";
    } else if (cmd == "heartbeat") {
        AdcId adc = 0;
        adc = readAdc(in);
        sendHeartbeat(w, adc);
        showStatusChanges(w);
    } else if (cmd == "silence" || cmd == "resume") {
        AdcId adc = 0;
        adc = readAdc(in);
        if (cmd == "silence") w.silenced.insert(adc); else w.silenced.erase(adc);
        std::cout << "  " << adcName(adc) << (cmd == "silence" ? " stopped" : " resumed") << " sending heartbeats\n";
    } else if (cmd == "add-adc") {
        AdcId adc = 0;
        adc = readAdc(in);
        if (adc <= 0) {
            std::cout << "  usage: add-adc ADC (e.g. ADC-3 or 3)\n";
            return true;
        }
        w.addAdc(adc);
        std::cout << "  " << adcName(adc) << " registered and monitored\n";
    } else if (cmd == "decommission") {
        AdcId adc = 0;
        adc = readAdc(in);
        w.registry.decommission(adc);
        w.heartbeats.stopMonitoring(adc);
        w.shownStatus.erase(adc);
        std::cout << "  " << adcName(adc) << " decommissioned: no longer monitored\n";
    } else if (cmd == "status") {
        for (const AdcId& adc : w.registry.activeAdcs()) {
            std::cout << "  " << adcName(adc) << " " << nameOf(w.heartbeats.status(adc), ADC_STATUS)
                      << (w.maintenance.isInMaintenance(adc, w.clock.now()) ? " (in maintenance)" : "")
                      << (w.silenced.count(adc) ? " (silenced)" : "") << "\n";
        }
    } else if (cmd == "user") {
        std::string role;
        in >> role;
        if (!parseRole(role, w.user)) std::cout << "  usage: user technician|pharmacist|administrator\n";
        std::cout << "  user is " << w.user.name << " (" << nameOf(w.user.role, ROLE) << ")\n";
    } else if (cmd == "schedule") {
        AdcId adc = 0;
        long long from = 0, to = 0;
        adc = readAdc(in);
        in >> from >> to;
        long long start = w.clock.now() + from;
        ScheduleVerdict v = w.maintenance.schedule(w.user, adc, start, w.clock.now() + to);
        std::cout << "  " << nameOf(v, SCHEDULE);
        if (v == ScheduleVerdict::Created) {
            std::cout << ": " << w.format.formatTime(start) << " to " << w.format.formatTime(w.clock.now() + to)
                      << " (cancel with: cancel " << adcName(adc) << " " << start << ")";
        }
        std::cout << "\n";
        showStatusChanges(w);
    } else if (cmd == "cancel") {
        AdcId adc = 0;
        long long start = 0;
        adc = readAdc(in);
        in >> start;
        std::cout << "  " << nameOf(w.maintenance.cancel(w.user, adc, start), CANCEL) << "\n";
    } else if (cmd == "alarm") {
        AdcId adc = 0;
        std::string typeName;
        AlarmType type;
        adc = readAdc(in);
        in >> typeName;
        if (parseAlarmType(typeName, type)) w.notifier.raise(adc, type, restOfLine(in));
        else std::cout << "  usage: alarm ADC offline|connection|format|lowstock|unauthorized [DETAIL]\n";
    } else if (cmd == "alarms") {
        showAlarms(w);
    } else if (cmd == "ack") {
        AlarmId id = 0;
        in >> id;
        std::cout << "  " << nameOf(w.alarms.acknowledge(w.user, id), ACK) << "\n";
    } else if (cmd == "resolve") {
        AlarmId id = 0;
        in >> id;
        std::cout << "  " << nameOf(w.alarms.resolve(w.user, id, restOfLine(in)), RESOLVE) << "\n";
    } else if (cmd == "stock") {
        for (const StockItem& item : w.inventory.items()) {
            std::cout << "  " << adcName(item.adc) << " " << item.medication << ": " << item.count << " of "
                      << item.capacity << "\n";
        }
    } else if (cmd == "hospital") {
        std::string med;
        std::vector<HospitalStockLevel> levels;
        if (in >> med) levels.push_back(w.hospital.check(med));
        else levels = w.hospital.all();
        for (const HospitalStockLevel& m : levels) {
            std::cout << "  " << m.medication << ": " << m.totalCount << " of " << m.totalCapacity
                      << " (reserve " << m.reserveCount << " of " << m.reserveCapacity << ", ADCs "
                      << m.adcCount << " of " << m.adcCapacity << ") " << nameOf(m.verdict, HOSPITAL)
                      << ", alarm: " << hospitalAlarm(w, m.medication) << "\n";
        }
    } else if (cmd == "reserve") {
        std::string med;
        int n = 0;
        in >> med >> n;
        ReserveVerdict v = w.hospital.manualReserveChange(w.user, med, n, restOfLine(in));
        std::cout << "  " << nameOf(v, RESERVE) << "\n";
        checkHospital(w);                                         // may print an alarm
    } else if (cmd == "withdraw") {
        AdcId adc = 0;
        std::string med;
        int n = 0;
        adc = readAdc(in);
        in >> med >> n;
        InventoryVerdict v = w.inventory.withdraw(adc, med, n);   // may print an alarm first
        std::cout << "  " << nameOf(v, INVENTORY) << "\n";
        checkHospital(w);                                         // may print an alarm
    } else if (cmd == "deposit") {
        AdcId adc = 0;
        std::string med;
        int n = 0;
        adc = readAdc(in);
        in >> med >> n;
        std::cout << "  " << nameOf(w.inventory.deposit(adc, med, n), INVENTORY) << "\n";
        checkHospital(w);                                         // may print an alarm
    } else if (cmd == "set") {
        AdcId adc = 0;
        std::string med;
        int n = 0;
        adc = readAdc(in);
        in >> med >> n;
        InventoryVerdict v = w.inventory.manualChange(w.user, adc, med, n, restOfLine(in));
        std::cout << "  " << nameOf(v, INVENTORY) << "\n";
        checkHospital(w);                                         // may print an alarm
    } else if (cmd == "report") {
        runDaily(w, true);
    } else if (cmd == "summary") {
        long long from = 0, to = 0;
        in >> from >> to;
        SummaryFilters filters{0, "", "", w.clock.now() + from, w.clock.now() + to};
        std::string word;
        while (in >> word) {
            if (word.rfind("adc=", 0) == 0)   filters.adc = parseAdc(word.substr(4));
            if (word.rfind("med=", 0) == 0)   filters.medication = word.substr(4);
            if (word.rfind("actor=", 0) == 0) filters.actor = word.substr(6);
        }
        SummaryResult r = w.reports.summary(w.user, filters);
        std::cout << "  " << nameOf(r.verdict, SUMMARY) << "\n";
        if (r.verdict == SummaryVerdict::Summarized) std::cout << w.format.formatSummary(r.summary);
    } else if (cmd == "history-fail") {
        w.historyFailing = !w.historyFailing;
        w.history.failWrites(w.historyFailing);
        w.history.failReads(w.historyFailing);
        std::cout << "  history is now " << (w.historyFailing ? "FAILING" : "working") << "\n";
    } else if (cmd == "sender-fail") {
        w.senderFailing = !w.senderFailing;
        w.sender.failSends(w.senderFailing);
        std::cout << "  report sending is now " << (w.senderFailing ? "FAILING" : "working") << "\n";
    } else {
        std::cout << "  unknown command; type help\n";
    }
    return true;
}

// ---- Scripted scenarios: each line is shown, then run like a typed command ----

static const std::vector<std::string> OFFLINE_SCENARIO = {
    "# Off-normal: ADC-1 stops sending heartbeats.",
    "status",
    "silence ADC-1",
    "# After 3 missed 60 s periods (180 s) ADC-1 goes Offline and AdcOffline is raised.",
    "advance 180",
    "alarms",
    "# Resolving without a note is rejected; with a note it is resolved and recorded.",
    "resolve 1",
    "resolve 1 replaced network cable on ADC-1",
    "alarms",
    "# A technician may not request an activity summary; a pharmacist may.",
    "summary -600 0 adc=ADC-1",
    "user pharmacist",
    "summary -600 0 adc=ADC-1",
};

static const std::vector<std::string> MAINTENANCE_SCENARIO = {
    "# Same silence, but ADC-2 is in a maintenance window for the next 600 s.",
    "schedule ADC-2 0 600",
    "silence ADC-2",
    "# ADC-2 goes Offline after 180 s, but no alarm is raised during maintenance.",
    "advance 300",
    "alarms",
    "# When the window ends and ADC-2 is still silent, AdcOffline is raised.",
    "advance 330",
    "alarms",
};

// Runs a scripted scenario line by line, as if each line were typed.
static void runScenario(World& w, const std::vector<std::string>& lines) {
    for (const std::string& line : lines) {
        std::cout << (line[0] == '#' ? "" : "> ") << line << "\n";
        run(w, line);
    }
}

// Starts the demo and reads commands until quit. `scenario` resets the world and
// runs a script, so each scenario starts from the same state.
int main() {
    auto world = std::make_unique<World>();
    std::cout << "HMS behaviour-hiding demo. Clock starts at "
              << world->format.formatTime(START_TIME) << "; ADC-1 and ADC-2 are online.\n";
    help();

    std::string line;
    while (std::cout << "\nhms> " && std::getline(std::cin, line)) {
        std::istringstream in(line);
        std::string cmd, name;
        in >> cmd >> name;
        if (cmd == "scenario") {
            if (name != "offline" && name != "maintenance") {
                std::cout << "  usage: scenario offline | scenario maintenance\n";
                continue;
            }
            world = std::make_unique<World>();
            std::cout << "  (world reset: clock " << world->format.formatTime(START_TIME)
                      << ", ADC-1 and ADC-2 online, user Tara (Technician))\n";
            runScenario(*world, name == "offline" ? OFFLINE_SCENARIO : MAINTENANCE_SCENARIO);
            continue;
        }
        if (!run(*world, line)) break;
    }
    return 0;
}
