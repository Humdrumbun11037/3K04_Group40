// Tests for the HMS side of the integration (integration/hms_side/live_adc.*):
// real encrypted packets into Subgroup B's integrated HMS (unchanged), the way
// the hms_live endpoint delivers them. No HTTP involved. Run from integration/.

#include "gui/integrated/sw_names.hpp"

#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "adc_side/adc_output_table.hpp"
#include "gui/integrated/int_api.hpp"
#include "hms_side/live_adc.hpp"

static int passed = 0;
static const char* const TEST_DATA = "build/test_data";

static std::vector<uint8_t> packet(IntWorld& w, const std::string& text) {
    return w.encryption.encryptForADC(std::vector<uint8_t>(text.begin(), text.end()));
}

static bool feedHas(const IntWorld& w, const std::string& text) {
    for (const FeedEntry& e : w.feed) {
        if (e.text.find(text) != std::string::npos) return true;
    }
    return false;
}

// The newest alarm in alarm resolution for this ADC and type, if any.
static std::optional<AlarmInfo> alarmFor(const IntWorld& w, int adc, AlarmType type) {
    std::optional<AlarmInfo> found;
    for (AlarmId id = 1; w.alarms.find(id).has_value(); id++) {
        AlarmInfo a = *w.alarms.find(id);
        if (a.adc == adc && a.type == type) found = a;
    }
    return found;
}

static bool inboxHas(const IntWorld& w, const std::string& user, const std::string& text) {
    for (const Inbox::Message& m : w.inbox.for_(user)) {
        if (m.text.find(text) != std::string::npos) return true;
    }
    return false;
}

struct Fixture {
    IntegratedApi api{TEST_DATA};
    LiveAdc link;
    Fixture() { LiveAdc::setup(api.world()); }
    IntWorld& w() { return api.world(); }
    std::string send(const std::string& text, long long ms = 0) { return link.receive(w(), packet(w(), text), ms); }
};

static void setup_registers_adc3_with_subgroup_a_stock() {
    Fixture f;
    assert(f.w().adcList.exists(3) && f.w().heartbeats.status(3) == AdcStatus::Online);
    assert(f.w().inventory.count(3, "Tylenol") == 100 && f.w().inventory.count(3, "Morphine") == 50);
    assert(f.w().silenced.count(3) == 0 && f.link.realAdcs().empty());   // simulated until a cabinet program picks it
    f.link.claim(f.w(), "3|Claim|0");                                      // a cabinet program picks it: now real
    assert(f.w().silenced.count(3) == 1 && f.link.realAdcs().count(3) == 1);
    LiveAdc::setup(f.w());   // a second call changes nothing
    assert(f.w().inventory.count(3, "Tylenol") == 100);
    passed++;
}

static void withdrawal_updates_count_history_and_raises_low_stock() {
    Fixture f;
    assert(f.send("3|Withdrawal|Tylenol|60|nurse_jane") == "Event");
    assert(f.w().inventory.count(3, "Tylenol") == 40);
    std::optional<AlarmInfo> low = alarmFor(f.w(), 3, AlarmType::InventoryLow);
    assert(low && low->detail == "Tylenol" && low->severity == Severity::High);
    assert(inboxHas(f.w(), "pharm_bob", "Tylenol"));   // High goes to the pharmacist
    bool logged = false;
    for (const auto& e : f.w().history.read(0, f.w().clock.now() + 1).entries) {
        logged = logged || (e.kind == "Withdrawal" && e.who == "nurse_jane" && e.adc == 3);
    }
    assert(logged);
    assert(!alarmFor(f.w(), 0, AlarmType::HospitalLowStock));   // the reserve covers it
    passed++;
}

static void deposit_and_over_withdrawal() {
    Fixture f;
    assert(f.send("3|Deposit|Morphine|20|pharm_bob") == "Event");
    assert(f.w().inventory.count(3, "Morphine") == 70);
    assert(f.send("3|Withdrawal|Morphine|90|nurse_jane") == "Event");
    assert(f.w().inventory.count(3, "Morphine") == 0);
    assert(alarmFor(f.w(), 3, AlarmType::InventoryDiscrepancy));
    passed++;
}

static void unknown_medication_is_refused_and_shown() {
    Fixture f;
    assert(f.send("3|Deposit|Aspirin|10|pharm_bob") == "Event");   // parsed and logged...
    assert(!f.w().inventory.count(3, "Aspirin"));                   // ...but not stocked at ADC-3
    assert(feedHas(f.w(), "refused Aspirin in ADC-3"));
    passed++;
}

static void unauthorized_access_is_tracked_and_resolvable() {
    Fixture f;
    f.send("3|Alarm|UNAUTHORIZED_ACCESS|ADC security lockout");
    std::optional<AlarmInfo> a = alarmFor(f.w(), 3, AlarmType::UnauthorizedAccess);
    assert(a && a->severity == Severity::Critical && a->state == AlarmState::Raised);
    assert(inboxHas(f.w(), "admin_alice", "UNAUTHORIZED_ACCESS") && inboxHas(f.w(), "tech_tom", "UNAUTHORIZED_ACCESS"));
    passed++;
}

static void jam_and_timeout_are_delivered_critical_but_not_tracked() {
    Fixture f;
    for (const char* code : {"HARDWARE_JAM", "TIMEOUT_FAULT"}) {
        assert(f.send(std::string("3|Alarm|") + code + "|detail") == "Event");
        assert(inboxHas(f.w(), "admin_alice", std::string("[CRITICAL] ADC 3 - ") + code));
        assert(feedHas(f.w(), std::string("ADC alarm code ") + code));   // "has no alarm type in alarm resolution"
    }
    assert(!f.w().alarms.find(1).has_value());   // nothing in alarm resolution
    passed++;
}

static void bad_packets_are_dropped_and_the_hms_keeps_running() {
    Fixture f;
    std::vector<uint8_t> tampered = packet(f.w(), "3|Withdrawal|Tylenol|5|x");
    tampered[14] ^= 0x01;
    assert(f.link.receive(f.w(), tampered, 0) == "Malformed");
    assert(f.link.receive(f.w(), {1, 2, 3}, 0) == "Malformed");
    assert(f.link.receive(f.w(), {}, 0) == "Malformed");
    assert(f.send("9|Withdrawal|Tylenol|5|x") == "UnknownAdc");
    assert(f.send("3|Withdrawal|Tylenol|99999999999|x") .rfind("RouterError", 0) == 0);   // caught, not thrown
    assert(f.w().inventory.count(3, "Tylenol") == 100);
    passed++;
}

static void silence_takes_adc3_offline_and_a_heartbeat_brings_it_back() {
    Fixture f;
    f.link.claim(f.w(), "3|Claim|0");   // the cabinet program drives it
    assert(f.send("3|Heartbeat") == "Heartbeat");
    f.w().advance(180);   // the simulator sends nothing for ADC-3
    assert(f.w().heartbeats.status(3) == AdcStatus::Offline);
    assert(alarmFor(f.w(), 3, AlarmType::AdcOffline));
    assert(f.w().heartbeats.status(1) == AdcStatus::Online);   // simulated ADCs unaffected
    assert(f.send("3|Heartbeat") == "Heartbeat");
    assert(f.w().heartbeats.status(3) == AdcStatus::Online);
    passed++;
}

static void advance_is_refused_only_while_the_link_is_live() {
    Fixture f;
    IntRequest advance{"POST", "/api/advance", {{"seconds", "180"}}};
    assert(!f.link.refuse(advance, 0));                 // nothing heard yet
    f.send("3|Heartbeat", 1000);
    assert(f.link.refuse(advance, 2000) && f.link.refuse(advance, 2000)->status == 409);
    assert(!f.link.refuse(advance, 1000 + LiveAdc::LIVE_MS));   // silent for 30 s
    f.link.cut = true;
    assert(!f.link.refuse(advance, 2000));
    assert(!f.link.refuse({"POST", "/api/resume", {{"adc", "ADC-3"}}}, 0));   // simulated: the simulator may resume it
    f.link.claim(f.w(), "3|Claim|0");
    assert(f.link.refuse({"POST", "/api/resume", {{"adc", "ADC-3"}}}, 0));
    assert(f.link.refuse({"POST", "/api/heartbeat", {{"adc", "3"}}}, 0));
    assert(!f.link.refuse({"POST", "/api/resume", {{"adc", "ADC-1"}}}, 0));
    passed++;
}

static void restart_and_scenarios_get_adc3_back() {
    Fixture f;
    f.link.claim(f.w(), "3|Claim|0");
    for (IntRequest r : {IntRequest{"POST", "/api/restart", {}}, IntRequest{"POST", "/api/scenario", {{"name", "lowstock"}}}}) {
        IntResponse answer = f.api.handle(r);
        f.link.after(f.api, r, answer);
        assert(f.w().adcList.exists(3) && f.w().inventory.count(3, "Tylenol") == 100);
        assert(f.link.realAdcs().count(3) == 1 && f.w().silenced.count(3) == 1);   // still the cabinet program's
    }
    passed++;
}

static bool has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

// Pins Subgroup B's Components text: if it changes, the correction would silently stop.
static void components_tab_lists_the_real_cabinets() {
    Fixture f;
    IntRequest state{"GET", "/api/state", {}};
    IntResponse answer = f.api.handle(state);
    assert(has(answer.body, LiveAdc::SIMULATED_ADCS_ENTRY));   // Subgroup B's entry, unchanged
    f.link.after(f.api, state, answer);
    assert(!has(answer.body, LiveAdc::SIMULATED_ADCS_ENTRY));
    assert(has(answer.body, R"({"name":"Real cabinets: none connected yet","owner":"Subgroup A / integration","real":true,)"));
    f.link.claim(f.w(), "1|Claim|0");
    f.link.claim(f.w(), "3|Claim|0");
    answer = f.api.handle(state);
    f.link.after(f.api, state, answer);
    assert(has(answer.body, R"({"name":"Real cabinets: ADC-1, ADC-3","owner":"Subgroup A / integration","real":true,)"));
    assert(has(answer.body, R"({"name":"Other ADCs and their link","owner":"Subgroup A / Matthew","real":false,)"));
    assert(has(answer.body, R"("name":"Clock","owner":"Matthew","real":false)"));   // the rest untouched
    assert(f.link.correctComponents("{\"ok\":true}") == "{\"ok\":true}");    // nothing to correct
    passed++;
}

// ---- Demo cabinet selection ----

static void cabinet_list_has_every_registered_adc() {
    Fixture f;
    assert(f.link.cabinets(f.w()) == "1|Ward 3 West|simulated\n2|ICU|simulated\n3|Ward 5|simulated\n");
    f.link.claim(f.w(), "3|Claim|0");
    assert(has(f.link.cabinets(f.w()), "3|Ward 5|real\n"));
    passed++;
}

static void claiming_a_simulated_adc_takes_it_over_with_the_hms_stock() {
    Fixture f;
    assert(f.link.claim(f.w(), "1|Claim|0") == "OK|Ward 3 West|Insulin=30;Morphine=40");
    assert(f.w().silenced.count(1) == 1 && f.link.realAdcs().count(1) == 1);   // no more simulated heartbeats
    assert(f.link.refuse({"POST", "/api/resume", {{"adc", "ADC-1"}}}, 0));
    assert(f.send("1|Withdrawal|Morphine|5|nurse_jane") == "Event" && f.w().inventory.count(1, "Morphine") == 35);
    passed++;
}

static void swapping_hands_the_previous_adc_back_to_the_simulator() {
    Fixture f;
    f.link.claim(f.w(), "1|Claim|0");
    assert(f.link.claim(f.w(), "3|Claim|1") == "OK|Ward 5|Morphine=50;Tylenol=100");
    assert(f.w().silenced.count(1) == 0 && f.link.realAdcs().count(1) == 0);
    assert(f.w().silenced.count(3) == 1 && f.link.realAdcs().count(3) == 1);
    assert(!f.link.refuse({"POST", "/api/resume", {{"adc", "ADC-1"}}}, 0));
    f.w().advance(180);
    assert(f.w().heartbeats.status(1) == AdcStatus::Online);   // the simulator kept it alive
    passed++;
}

static void an_adc_registered_during_the_demo_can_be_opened() {
    Fixture f;
    f.w().login("admin_alice", "hash000");   // registering is an Administrator permission (Inodh's settings)
    assert(f.api.handle({"POST", "/api/adc/register", {{"id", "5"}, {"location", "Emergency"}}}).status == 200);
    assert(has(f.link.cabinets(f.w()), "5|Emergency|simulated\n"));
    assert(f.link.claim(f.w(), "5|Claim|0") == "OK|Emergency|Morphine=50;Tylenol=100");   // Subgroup A's starting stock
    assert(f.send("5|Withdrawal|Tylenol|10|nurse_jane") == "Event" && f.w().inventory.count(5, "Tylenol") == 90);
    passed++;
}

static void bad_claims_are_refused() {
    Fixture f;
    assert(f.link.claim(f.w(), "9|Claim|0") == "ERR|ADC-9 is not registered on the HMS");
    assert(f.link.claim(f.w(), "List") == "ERR|not a claim");
    assert(f.link.claim(f.w(), "3|Withdrawal|0") == "ERR|not a claim");
    assert(f.link.realAdcs().empty());
    passed++;
}

static void hms_accounts_are_the_standard_ones() {
    Fixture f;
    using R = Authorization::LoginResult;
    assert(f.w().login("pharm_bob", "hash456") == R::Success && f.w().users.getUserRole("pharm_bob") == UserRole::Pharmacist);
    assert(f.w().login("tech_tom", "hash789") == R::Success && f.w().users.getUserRole("tech_tom") == UserRole::Technician);
    assert(f.w().login("admin_alice", "hash000") == R::Success
           && f.w().users.getUserRole("admin_alice") == UserRole::Administrator);
    for (const char* old : {"priya", "tara", "alex"}) assert(!f.w().users.exists(old));
    assert(!f.w().users.exists("nurse_jane"));   // no Nurse role on the HMS
    passed++;
}

// Subgroup B's scenarios log in as tara or priya before ADC-3 is set up again.
static void a_scenario_session_moves_to_the_standard_account() {
    Fixture f;
    IntRequest r{"POST", "/api/scenario", {{"name", "maintenance"}}};   // logs in as tara
    IntResponse answer = f.api.handle(r);
    assert(f.w().sessionUser == "tara");
    f.link.after(f.api, r, answer);
    assert(f.w().sessionUser == "tech_tom" && f.w().touchSession());
    assert(!f.w().users.exists("tara"));
    passed++;
}

// Pins Subgroup B's login hint: if it changes, the page would show the old accounts.
static void login_hint_shows_the_standard_accounts() {
    std::ifstream in("../Subgroup_B/gui/integrated/web/index.html", std::ios::binary);
    assert(in && "run from integration/");
    std::ostringstream page;
    page << in.rdbuf();
    assert(has(page.str(), LiveAdc::OLD_LOGIN_HINT));
    std::string corrected = LiveAdc::correctPage(page.str());
    assert(!has(corrected, "tara") && has(corrected, LiveAdc::LOGIN_HINT));
    assert(corrected.size() == page.str().size() - std::string(LiveAdc::OLD_LOGIN_HINT).size()
                               + std::string(LiveAdc::LOGIN_HINT).size());   // nothing else touched
    passed++;
}

static int alarmCount(const IntWorld& w) {
    int n = 0;
    while (w.alarms.find(n + 1).has_value()) n++;
    return n;
}

static void unreadable_packets_raise_incorrect_message_format() {
    Fixture f;
    EncryptionManager other(EncryptionManager::defaultStrategy(), EncryptionManager::generateKey());
    std::string text = "3|Heartbeat";
    assert(f.link.receive(f.w(), other.encryptForADC({text.begin(), text.end()}), 0) == "Malformed");
    std::optional<AlarmInfo> a = alarmFor(f.w(), 0, AlarmType::IncorrectMessageFormat);   // sender unknown
    assert(a && a->severity == Severity::Critical && a->detail.find("could not be decrypted") != std::string::npos);
    assert(f.send("3|NotAKeyword|x") == "NotAnEvent");                                   // authentic, wrong format
    assert(alarmFor(f.w(), 3, AlarmType::IncorrectMessageFormat));
    assert(f.send("3") == "Malformed");                                                  // too short
    int before = alarmCount(f.w());
    f.send("3|NotAKeyword|y");                                                           // repeat joins the open alarm
    assert(alarmCount(f.w()) == before);
    assert(inboxHas(f.w(), "admin_alice", "INCORRECT_MESSAGE_FORMAT"));
    f.send("3|Heartbeat");
    f.send("9|Heartbeat");                                                               // unknown ADC: dropped, no alarm
    assert(alarmCount(f.w()) == before);
    passed++;
}

static void late_message_is_noted_and_recorded_with_its_adc_time() {
    Fixture f;
    std::string stamped = adc_output::stamp("3|Withdrawal|Tylenol|5|nurse_jane", "2026-11-07 10:00:00");
    assert(stamped == "3|Withdrawal|Tylenol|5|nurse_jane|ADC_TIME=2026-11-07 10:00:00");
    assert(f.link.receive(f.w(), packet(f.w(), stamped), 0, "2026-11-07 10:00:30") == "Event");
    assert(f.w().inventory.count(3, "Tylenol") == 95);   // the stamp doesn't disturb Inodh's parser
    assert(feedHas(f.w(), "happened at 2026-11-07 10:00:00 ADC time, arrived 30 s later"));
    bool recorded = false;
    for (const auto& e : f.w().history.read(0, f.w().clock.now() + 1).entries) {
        recorded = recorded || (e.kind == "AdcLateMessage" && e.adc == 3
                                && e.detail == "Withdrawal from ADC-3 happened at 2026-11-07 10:00:00 ADC local time and arrived 30 s later");
    }
    assert(recorded);
    std::size_t feedBefore = f.w().feed.size();
    f.link.receive(f.w(), packet(f.w(), adc_output::stamp("3|Withdrawal|Tylenol|5|nurse_jane", "2026-11-07 10:01:00")), 0,
                   "2026-11-07 10:01:02");   // on time: no late note
    assert(!feedHas(f.w(), "10:01:00 ADC time, arrived"));
    assert(f.w().feed.size() > feedBefore);
    assert(adc_output::stampOf("3|Heartbeat").empty());
    passed++;
}

// The contract between the two halves: every message the ADC side can make is
// one the real HMS parser accepts as an event.
static void every_table_message_is_an_hms_event() {
    Fixture f;
    const std::string log = "[SW-2 EVENT LOG SAVED]: [TIMESTAMP] ";
    const std::string lines[] = {
        log + "nurse_jane dispensed 10mg of Tylenol",
        log + "pharm_bob restocked 5mg of Morphine",
        "[HW-3] CRITICAL: Security Relay triggered! Hospital Staff Notified!",
        log + "CRITICAL FAULT: Drawer 2 left open.",
        log + "nurse_jane attempted to dispense 5mg of Morphine but caused a HARDWARE JAM",
        "Simulating physical obstruction in drawer...",
        "Simulating HMS connection drop...",
    };
    for (const std::string& line : lines) {
        std::optional<std::string> message = adc_output::translate(LiveAdc::ADC_ID, line);
        assert(message && f.send(*message) == "Event");
        assert(f.send(adc_output::stamp(*message, "2026-11-07 10:00:00")) == "Event");   // as the link sends it
    }
    assert(alarmFor(f.w(), 3, AlarmType::ConnectionLoss));
    passed++;
}

int main() {
    setup_registers_adc3_with_subgroup_a_stock();
    withdrawal_updates_count_history_and_raises_low_stock();
    deposit_and_over_withdrawal();
    unknown_medication_is_refused_and_shown();
    unauthorized_access_is_tracked_and_resolvable();
    jam_and_timeout_are_delivered_critical_but_not_tracked();
    bad_packets_are_dropped_and_the_hms_keeps_running();
    silence_takes_adc3_offline_and_a_heartbeat_brings_it_back();
    advance_is_refused_only_while_the_link_is_live();
    restart_and_scenarios_get_adc3_back();
    components_tab_lists_the_real_cabinets();
    cabinet_list_has_every_registered_adc();
    claiming_a_simulated_adc_takes_it_over_with_the_hms_stock();
    swapping_hands_the_previous_adc_back_to_the_simulator();
    an_adc_registered_during_the_demo_can_be_opened();
    bad_claims_are_refused();
    hms_accounts_are_the_standard_ones();
    a_scenario_session_moves_to_the_standard_account();
    login_hint_shows_the_standard_accounts();
    unreadable_packets_raise_incorrect_message_format();
    late_message_is_noted_and_recorded_with_its_adc_time();
    every_table_message_is_an_hms_event();
    std::cout << "live_adc_test: " << passed << " tests passed\n";
    return 0;
}
