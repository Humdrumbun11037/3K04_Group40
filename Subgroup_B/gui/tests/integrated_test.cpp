// Tests for the integrated program (gui/integrated/): Andrew's, Inodh's and
// Matthew's real code working together through the adapters. No HTTP involved.

#include "gui/integrated/sw_names.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>

#include "gui/integrated/int_api.hpp"

namespace hb = hms::behaviour;

static int passed = 0;
static const char* const TEST_DATA = "data/test";   // never the demo's gui/data

static IntResponse get(IntegratedApi& api, const std::string& path, std::map<std::string, std::string> params = {}) {
    return api.handle({"GET", path, params});
}

static IntResponse post(IntegratedApi& api, const std::string& path, std::map<std::string, std::string> params = {}) {
    return api.handle({"POST", path, params});
}

static bool has(const IntResponse& r, const std::string& text) {
    return r.body.find(text) != std::string::npos;
}

static IntResponse login(IntegratedApi& api, const std::string& user, const std::string& password) {
    return post(api, "/api/login", {{"user", user}, {"password", password}});
}

// ---- Step 3: Inodh's ADC registry ----

static void registry_starts_with_two_located_adcs() {
    IntegratedApi api(TEST_DATA);
    IntResponse s = get(api, "/api/state");
    assert(has(s, "\"location\":\"Ward 3 West\"") && has(s, "\"location\":\"ICU\""));
    assert(api.world().registry.isActive(1) && api.world().registry.isActive(2));
    assert(!api.world().registry.isActive(0) && !api.world().registry.isActive(3));
    passed++;
}

static void registry_changes_follow_inodhs_permission_table() {
    IntegratedApi api(TEST_DATA);
    login(api, "tara", "tara-demo");
    IntResponse r = post(api, "/api/adc/register", {{"id", "3"}, {"location", "ER"}});
    assert(has(r, "\"verdict\":\"NotAuthorized\""));
    assert(!api.world().adcList.exists(3));
    passed++;
}

static void admin_registers_relocates_and_removes() {
    IntegratedApi api(TEST_DATA);
    login(api, "alex", "alex-demo");
    assert(has(post(api, "/api/adc/register", {{"id", "3"}, {"location", "ER"}}), "\"verdict\":\"Ok\""));
    assert(api.world().heartbeats.status(3) == AdcStatus::Online);    // monitoring follows the registry
    assert(has(post(api, "/api/adc/relocate", {{"id", "3"}, {"location", "Day Surgery"}}), "\"verdict\":\"Ok\""));
    assert(api.world().adcList.find(3)->location == "Day Surgery");
    assert(has(post(api, "/api/adc/remove", {{"id", "3"}}), "\"verdict\":\"Ok\""));
    assert(api.world().heartbeats.status(3) == AdcStatus::NotMonitored);
    passed++;
}

static void registry_rejections_use_inodhs_verdicts() {
    IntegratedApi api(TEST_DATA);
    login(api, "alex", "alex-demo");
    assert(has(post(api, "/api/adc/register", {{"id", "1"}, {"location", "X"}}), "\"verdict\":\"DuplicateId\""));
    assert(has(post(api, "/api/adc/register", {{"id", "0"}, {"location", "X"}}), "\"verdict\":\"InvalidId\""));
    assert(has(post(api, "/api/adc/register", {{"id", "4"}, {"location", "  "}}), "\"verdict\":\"InvalidLocation\""));
    assert(has(post(api, "/api/adc/remove", {{"id", "9"}}), "\"verdict\":\"NotFound\""));
    assert(post(api, "/api/adc/register", {{"id", "x"}}).status == 400);
    passed++;
}

static void registry_changes_are_in_history_with_the_adc() {
    IntegratedApi api(TEST_DATA);
    login(api, "alex", "alex-demo");
    post(api, "/api/adc/register", {{"id", "3"}, {"location", "ER"}});
    IntResponse h = get(api, "/api/history", {{"kind", "AdcRegistry"}, {"adc", "3"}});
    assert(has(h, "\"who\":\"alex\"") && has(h, "action=registered location=ER"));
    passed++;
}

static void removed_adc_never_alarms() {
    IntegratedApi api(TEST_DATA);
    login(api, "alex", "alex-demo");
    post(api, "/api/adc/remove", {{"id", "2"}});
    post(api, "/api/advance", {{"seconds", "600"}});
    assert(!has(get(api, "/api/state"), "\"type\":\"AdcOffline\""));
    passed++;
}

static void maintenance_uses_the_real_registry() {
    IntegratedApi api(TEST_DATA);
    login(api, "tara", "tara-demo");   // Technician may schedule (agreed table)
    assert(has(post(api, "/api/maintenance/schedule", {{"adc", "2"}, {"startMin", "0"}, {"lengthMin", "10"}}),
               "\"verdict\":\"Created\""));
    assert(has(post(api, "/api/maintenance/schedule", {{"adc", "7"}, {"startMin", "0"}, {"lengthMin", "10"}}),
               "\"verdict\":\"UnknownAdc\""));
    passed++;
}

static void components_panel_lists_real_and_simulated() {
    IntegratedApi api(TEST_DATA);
    IntResponse s = get(api, "/api/state");
    assert(has(s, "\"name\":\"ADC registry\",\"owner\":\"Inodh\",\"real\":true"));
    assert(has(s, "\"name\":\"Clock\",\"owner\":\"Matthew\",\"real\":false"));
    passed++;
}

// ---- Step 2: Inodh's login, users, sessions and user settings ----

static void demo_accounts_log_in_with_their_roles() {
    IntegratedApi api(TEST_DATA);
    assert(has(login(api, "tara", "tara-demo"), "\"verdict\":\"Success\""));
    assert(api.world().user.role == hb::Role::Technician);
    assert(has(login(api, "priya", "priya-demo"), "\"verdict\":\"Success\""));
    assert(api.world().user.role == hb::Role::Pharmacist);
    assert(has(login(api, "alex", "alex-demo"), "\"verdict\":\"Success\""));
    assert(has(get(api, "/api/state"), "\"role\":\"Administrator\""));
    passed++;
}

static void wrong_password_gives_one_answer_for_id_or_password() {
    IntegratedApi api(TEST_DATA);
    IntResponse a = login(api, "tara", "wrong");
    IntResponse b = login(api, "nobody", "wrong");
    assert(has(a, "\"verdict\":\"InvalidCredentials\"") && has(b, "\"verdict\":\"InvalidCredentials\""));
    passed++;
}

static void fourth_failure_locks_the_account() {
    IntegratedApi api(TEST_DATA);
    for (int i = 0; i < 3; i++) assert(has(login(api, "tara", "wrong"), "\"verdict\":\"InvalidCredentials\""));
    assert(has(login(api, "tara", "wrong"), "\"verdict\":\"AccountLocked\""));
    assert(has(login(api, "tara", "tara-demo"), "\"verdict\":\"AccountLocked\""));   // even the right password
    assert(api.world().users.isAccountLocked("tara"));
    passed++;
}

static void administrator_unlocks_an_account() {
    IntegratedApi api(TEST_DATA);
    for (int i = 0; i < 4; i++) login(api, "priya", "wrong");
    login(api, "alex", "alex-demo");
    assert(has(post(api, "/api/users/unlock", {{"user", "priya"}}), "\"ok\":true"));
    assert(has(login(api, "priya", "priya-demo"), "\"verdict\":\"Success\""));
    passed++;
}

static void user_actions_need_a_login_and_state_hides_data() {
    IntegratedApi api(TEST_DATA);
    assert(post(api, "/api/alarm/ack", {{"id", "1"}}).status == 401);
    assert(post(api, "/api/stock/set", {{"adc", "1"}, {"med", "Morphine"}, {"count", "5"}}).status == 401);
    assert(get(api, "/api/history").status == 401);
    IntResponse s = get(api, "/api/state");
    assert(has(s, "\"loggedIn\":false") && !has(s, "\"stock\"") && !has(s, "\"alarms\""));
    assert(post(api, "/api/advance", {{"seconds", "60"}}).status == 200);   // simulation works without a login
    passed++;
}

static void session_ends_after_15_idle_minutes() {
    IntegratedApi api(TEST_DATA);
    login(api, "priya", "priya-demo");
    post(api, "/api/advance", {{"seconds", "901"}});
    assert(post(api, "/api/report/summary", {{"hours", "1"}}).status == 401);
    assert(has(get(api, "/api/state"), "\"loggedIn\":false"));
    passed++;
}

static void activity_keeps_the_session_alive() {
    IntegratedApi api(TEST_DATA);
    login(api, "priya", "priya-demo");
    post(api, "/api/advance", {{"seconds", "600"}});
    assert(post(api, "/api/report/summary", {{"hours", "1"}}).status == 200);
    post(api, "/api/advance", {{"seconds", "600"}});
    assert(post(api, "/api/report/summary", {{"hours", "1"}}).status == 200);
    passed++;
}

static void only_administrators_manage_users() {
    IntegratedApi api(TEST_DATA);
    login(api, "priya", "priya-demo");
    assert(has(post(api, "/api/users/add", {{"user", "sam"}, {"password", "pw"}, {"role", "Technician"}}),
               "\"verdict\":\"NotAuthorized\""));
    login(api, "alex", "alex-demo");
    assert(has(post(api, "/api/users/add", {{"user", "sam"}, {"password", "pw"}, {"role", "Technician"}}), "\"ok\":true"));
    assert(has(post(api, "/api/users/add", {{"user", "sam"}, {"password", "pw"}, {"role", "Technician"}}), "\"ok\":false"));
    assert(has(login(api, "sam", "pw"), "\"verdict\":\"Success\""));
    login(api, "alex", "alex-demo");
    assert(has(post(api, "/api/users/remove", {{"user", "sam"}}), "\"ok\":true"));
    assert(has(login(api, "sam", "pw"), "\"verdict\":\"InvalidCredentials\""));
    passed++;
}

static void cannot_remove_own_account() {
    IntegratedApi api(TEST_DATA);
    login(api, "alex", "alex-demo");
    assert(has(post(api, "/api/users/remove", {{"user", "alex"}}), "\"ok\":false"));
    assert(api.world().users.exists("alex"));
    passed++;
}

static void alarm_recipients_follow_inodhs_chart() {
    IntegratedApi api(TEST_DATA);
    login(api, "alex", "alex-demo");
    IntResponse s = get(api, "/api/state");
    assert(has(s, "\"severity\":\"CRITICAL\",\"to\":[{\"role\":\"Administrator\",\"users\":\"alex\"},"
                  "{\"role\":\"Technician\",\"users\":\"tara\"}]"));
    assert(has(s, "\"severity\":\"HIGH\",\"to\":[{\"role\":\"Pharmacist\",\"users\":\"priya\"}]"));
    passed++;
}

static void login_attempts_are_recorded_in_history() {
    IntegratedApi api(TEST_DATA);
    login(api, "tara", "wrong");
    login(api, "alex", "alex-demo");
    IntResponse h = get(api, "/api/history", {{"kind", "UserLogin"}});
    assert(has(h, "\"who\":\"tara\"") && has(h, "result=failed"));
    assert(has(h, "\"who\":\"alex\"") && has(h, "result=success"));
    passed++;
}

static void andrews_modules_use_the_session_role_and_agreed_table() {
    IntegratedApi api(TEST_DATA);
    login(api, "tara", "tara-demo");
    assert(has(post(api, "/api/stock/set", {{"adc", "1"}, {"med", "Morphine"}, {"count", "50"}, {"explanation", "count"}}),
               "\"verdict\":\"NotAuthorized\""));
    assert(has(post(api, "/api/maintenance/schedule", {{"adc", "1"}, {"startMin", "0"}, {"lengthMin", "5"}}),
               "\"verdict\":\"Created\""));   // agreed table: Technician (Inodh's own table says Administrator)
    login(api, "priya", "priya-demo");
    assert(has(post(api, "/api/stock/set", {{"adc", "1"}, {"med", "Morphine"}, {"count", "50"}, {"explanation", "count"}}),
               "\"verdict\":\"Updated\""));
    IntResponse h = get(api, "/api/history", {{"kind", "InventoryManualChange"}});
    assert(has(h, "\"who\":\"priya\""));
    passed++;
}

static void logout_ends_the_session() {
    IntegratedApi api(TEST_DATA);
    login(api, "priya", "priya-demo");
    post(api, "/api/logout");
    assert(post(api, "/api/report/summary", {{"hours", "1"}}).status == 401);
    passed++;
}

// ---- Step 4: Inodh's message path into Andrew's modules ----

static IntResponse send(IntegratedApi& api, const std::string& text) {
    return post(api, "/api/message/send", {{"text", text}});
}

static int alarmCount(IntegratedApi& api, AlarmType type) {
    int n = 0;
    for (AlarmId id = 1; api.world().alarms.find(id).has_value(); id++) {
        if (api.world().alarms.find(id)->type == type) n++;
    }
    return n;
}

static void heartbeats_arrive_as_encrypted_packets() {
    IntegratedApi api(TEST_DATA);
    post(api, "/api/advance", {{"seconds", "600"}});
    assert(api.world().heartbeats.status(1) == AdcStatus::Online);
    assert(api.world().lastHeartbeat[1] == IntWorld::START_TIME + 600);
    assert(api.world().queued() == 0);
    passed++;
}

static void patient_field_is_stripped_before_anything_sees_it() {
    IntegratedApi api(TEST_DATA);
    IntResponse r = send(api, "1|Withdrawal|Morphine|5|U1001|PT_NAME=Jane Doe|PT_MRN=00012345");
    assert(has(r, "\"outcome\":\"Event\""));
    assert(has(r, "\"removed\":[\"PT_NAME=Jane Doe\",\"PT_MRN=00012345\"]"));
    assert(has(r, "\"stripped\":\"1|Withdrawal|Morphine|5|U1001\""));
    assert(*api.world().inventory.count(1, "Morphine") == 35);
    hb::HistoryReadResult all = api.world().history.read(IntWorld::START_TIME - 86400, api.world().clock.now() + 1);
    assert(all.verdict == hb::ReadVerdict::Ok && !all.entries.empty());
    for (const hb::HistoryEntry& e : all.entries) {
        assert(e.detail.find("Jane") == std::string::npos && e.detail.find("00012345") == std::string::npos);
    }
    passed++;
}

static void deposit_message_reaches_inventory_record() {
    IntegratedApi api(TEST_DATA);
    assert(has(send(api, "2|Deposit|Morphine|15|U2002"), "\"outcome\":\"Event\""));
    assert(*api.world().inventory.count(2, "Morphine") == 75);
    passed++;
}

static void silent_adc_alarm_is_delivered_and_tracked() {
    IntegratedApi api(TEST_DATA);
    post(api, "/api/silence", {{"adc", "1"}});
    post(api, "/api/advance", {{"seconds", "180"}});
    assert(alarmCount(api, AlarmType::AdcOffline) == 1);
    AlarmInfo a = *api.world().alarms.find(1);
    assert(a.severity == Severity::Critical && a.destination == hb::Role::Administrator);
    assert(api.world().inbox.for_("alex").size() == 1 && api.world().inbox.for_("tara").size() == 1);   // Critical: both
    assert(api.world().inbox.for_("priya").empty());
    passed++;
}

static void adc_alarm_message_reaches_alarm_resolution() {
    IntegratedApi api(TEST_DATA);
    assert(has(send(api, "1|Alarm|UNAUTHORIZED_ACCESS|badge reader forced"), "\"outcome\":\"Event\""));
    assert(alarmCount(api, AlarmType::UnauthorizedAccess) == 1);
    assert(api.world().alarms.find(1)->detail == "badge reader forced");
    passed++;
}

static void unknown_alarm_code_is_delivered_but_not_tracked() {
    IntegratedApi api(TEST_DATA);
    send(api, "1|Alarm|TEMPERATURE|Fridge above 8C");
    assert(!api.world().alarms.find(1).has_value());
    assert(api.world().links.unmatched.size() == 1);
    assert(api.world().inbox.for_("alex").size() == 1);   // unknown codes are Critical
    login(api, "alex", "alex-demo");
    assert(has(get(api, "/api/state"), "\"alarm\":\"none (unknown code)\""));
    passed++;
}

static void over_withdrawal_raises_one_discrepancy() {
    IntegratedApi api(TEST_DATA);
    send(api, "2|Withdrawal|Morphine|70|U2002");
    assert(*api.world().inventory.count(2, "Morphine") == 0);
    assert(alarmCount(api, AlarmType::InventoryDiscrepancy) == 1);
    assert(alarmCount(api, AlarmType::InventoryLow) == 1);
    passed++;
}

static void bad_messages_are_dropped() {
    IntegratedApi api(TEST_DATA);
    assert(has(send(api, "garbage"), "\"outcome\":\"Malformed\""));
    assert(has(send(api, "9|Heartbeat"), "\"outcome\":\"UnknownAdc\""));
    assert(has(send(api, "1|Hello"), "\"outcome\":\"NotAnEvent\""));
    assert(post(api, "/api/message/send", {{"text", ""}}).status == 400);
    passed++;
}

static void router_exception_does_not_stop_the_hms() {
    IntegratedApi api(TEST_DATA);
    IntResponse r = send(api, "1|Withdrawal|Morphine|99999999999|U1001");   // quantity too big for int
    assert(has(r, "RouterError"));
    assert(*api.world().inventory.count(1, "Morphine") == 40);
    assert(has(send(api, "1|Withdrawal|Morphine|1|U1001"), "\"outcome\":\"Event\""));
    passed++;
}

static void unknown_medication_is_refused_by_inventory_record() {
    IntegratedApi api(TEST_DATA);
    send(api, "1|Deposit|Aspirin|5|U1001");
    assert(!api.world().inventory.count(1, "Aspirin").has_value());
    assert(api.world().feed.back().text.find("UnknownItem") != std::string::npos
           || api.world().feed[api.world().feed.size() - 2].text.find("UnknownItem") != std::string::npos);
    passed++;
}

static void resolving_clears_the_notification() {
    IntegratedApi api(TEST_DATA);
    send(api, "1|Alarm|UNAUTHORIZED_ACCESS|badge reader forced");
    assert(api.world().swNotifier.pending().size() == 1);
    login(api, "alex", "alex-demo");
    post(api, "/api/alarm/resolve", {{"id", "1"}, {"note", "door repaired"}});
    assert(api.world().swNotifier.pending().empty());
    passed++;
}

static void escalation_is_delivered_to_dashboards() {
    IntegratedApi api(TEST_DATA);
    post(api, "/api/silence", {{"adc", "1"}});
    post(api, "/api/advance", {{"seconds", "300"}});
    assert(api.world().alarms.find(1)->escalated);
    bool found = false;
    for (const Inbox::Message& m : api.world().inbox.for_("alex")) found = found || m.text.find("[ESCALATED]") == 0;
    assert(found);
    passed++;
}

static void inbox_shown_to_the_logged_in_user() {
    IntegratedApi api(TEST_DATA);
    send(api, "1|Alarm|UNAUTHORIZED_ACCESS|badge reader forced");
    login(api, "tara", "tara-demo");
    assert(has(get(api, "/api/state"), "UNAUTHORIZED_ACCESS: badge reader forced"));
    login(api, "priya", "priya-demo");
    assert(!has(get(api, "/api/state"), "UNAUTHORIZED_ACCESS: badge reader forced"));
    passed++;
}

static void withdraw_upload_goes_through_the_router() {
    IntegratedApi api(TEST_DATA);
    IntResponse r = post(api, "/api/stock/withdraw", {{"adc", "1"}, {"med", "Morphine"}, {"amount", "5"}});
    assert(has(r, "\"verdict\":\"Event\"") && has(r, "\"stripped\":\"1|Withdrawal|Morphine|5|ADC-USER\""));
    assert(*api.world().inventory.count(1, "Morphine") == 35);
    login(api, "alex", "alex-demo");
    assert(has(get(api, "/api/history", {{"kind", "Withdrawal"}}), "quantity=5"));   // Inodh's event log
    passed++;
}

// ---- Step 5: Matthew's encrypted storage behind Andrew's History ----

static std::string fileText(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}

static void history_is_stored_encrypted_on_disk() {
    IntegratedApi api(TEST_DATA);
    send(api, "1|Withdrawal|Morphine|5|U1001|PT_NAME=Jane Doe");
    login(api, "priya", "priya-demo");
    std::string disk = fileText(std::string(TEST_DATA) + "/active.log");
    assert(!disk.empty());
    for (const char* plain : {"Morphine", "Withdrawal", "priya", "UserLogin", "Jane", "ADC-1"}) {
        assert(disk.find(plain) == std::string::npos);
    }
    assert(disk.find_first_not_of("0123456789ABCDEF\r\n") == std::string::npos);   // hex lines only
    passed++;
}

static void history_search_reads_the_file() {
    IntegratedApi api(TEST_DATA);
    login(api, "priya", "priya-demo");
    post(api, "/api/stock/set", {{"adc", "1"}, {"med", "Insulin"}, {"count", "12"}, {"explanation", "stock take"}});
    IntResponse h = get(api, "/api/history", {{"med", "Insulin"}});
    assert(has(h, "\"kind\":\"InventoryManualChange\"") && has(h, "stock take") && has(h, "\"adc\":\"ADC-1\""));
    passed++;
}

static void daily_report_reads_the_file() {
    IntegratedApi api(TEST_DATA);
    post(api, "/api/scenario", {{"name", "daily"}});
    login(api, "alex", "alex-demo");
    IntResponse r = post(api, "/api/report/daily");
    assert(has(r, "\"verdict\":\"AlreadyDone\""));
    assert(has(r, "AdcRegistry action=registered location=Ward 3 West"));   // stored at start-up, read back
    assert(has(r, "ADC-2: "));                                                // flagged for missing heartbeats
    passed++;
}

static void real_write_failure_becomes_a_verdict() {
    IntegratedApi api(TEST_DATA);
    login(api, "priya", "priya-demo");
    std::string active = std::string(TEST_DATA) + "/active.log";
    std::filesystem::remove(active);
    std::filesystem::create_directory(active);   // the file can no longer be opened for writing
    IntResponse r = post(api, "/api/stock/set", {{"adc", "1"}, {"med", "Morphine"}, {"count", "10"}, {"explanation", "count"}});
    assert(has(r, "\"verdict\":\"StorageFailure\""));
    assert(*api.world().inventory.count(1, "Morphine") == 40);   // not changed: it could not be recorded
    assert(api.world().history.realFailures > 0);
    std::filesystem::remove(active);
    assert(has(post(api, "/api/stock/set", {{"adc", "1"}, {"med", "Morphine"}, {"count", "10"}, {"explanation", "count"}}),
               "\"verdict\":\"Updated\""));
    passed++;
}

static void corrupted_record_becomes_a_read_failure() {
    IntegratedApi api(TEST_DATA);
    login(api, "alex", "alex-demo");
    {
        std::ofstream out(std::string(TEST_DATA) + "/active.log", std::ios::app);
        out << "ZZ-not-a-record\n";
    }
    IntResponse h = get(api, "/api/history");
    assert(has(h, "\"ok\":false") && has(h, "StorageFailure"));
    assert(has(post(api, "/api/report/summary", {{"hours", "1"}}), "\"verdict\":\"ReadFailure\""));
    passed++;
}

static void restart_keeps_the_key_and_starts_a_fresh_trail() {
    IntegratedApi api(TEST_DATA);
    std::string key = fileText(std::string(TEST_DATA) + "/key.bin");
    assert(key.size() == 32);
    login(api, "alex", "alex-demo");
    for (int i = 0; i < 5; i++) post(api, "/api/advance", {{"seconds", "60"}});
    post(api, "/api/restart");
    assert(fileText(std::string(TEST_DATA) + "/key.bin") == key);
    login(api, "alex", "alex-demo");
    IntResponse h = get(api, "/api/history", {{"kind", "UserLogin"}});
    assert(has(h, "\"ok\":true") && has(h, "1 history entries"));   // only the login after the restart
    passed++;
}

static void simulated_failure_switch_still_works() {
    IntegratedApi api(TEST_DATA);
    login(api, "priya", "priya-demo");
    post(api, "/api/fail/history", {{"on", "1"}});
    assert(has(post(api, "/api/stock/set", {{"adc", "1"}, {"med", "Morphine"}, {"count", "10"}, {"explanation", "x"}}),
               "\"verdict\":\"StorageFailure\""));
    assert(api.world().history.realFailures == 0);
    passed++;
}

int main() {
    registry_starts_with_two_located_adcs();
    registry_changes_follow_inodhs_permission_table();
    admin_registers_relocates_and_removes();
    registry_rejections_use_inodhs_verdicts();
    registry_changes_are_in_history_with_the_adc();
    removed_adc_never_alarms();
    maintenance_uses_the_real_registry();
    components_panel_lists_real_and_simulated();
    demo_accounts_log_in_with_their_roles();
    wrong_password_gives_one_answer_for_id_or_password();
    fourth_failure_locks_the_account();
    administrator_unlocks_an_account();
    user_actions_need_a_login_and_state_hides_data();
    session_ends_after_15_idle_minutes();
    activity_keeps_the_session_alive();
    only_administrators_manage_users();
    cannot_remove_own_account();
    alarm_recipients_follow_inodhs_chart();
    login_attempts_are_recorded_in_history();
    andrews_modules_use_the_session_role_and_agreed_table();
    logout_ends_the_session();
    heartbeats_arrive_as_encrypted_packets();
    patient_field_is_stripped_before_anything_sees_it();
    deposit_message_reaches_inventory_record();
    silent_adc_alarm_is_delivered_and_tracked();
    adc_alarm_message_reaches_alarm_resolution();
    unknown_alarm_code_is_delivered_but_not_tracked();
    over_withdrawal_raises_one_discrepancy();
    bad_messages_are_dropped();
    router_exception_does_not_stop_the_hms();
    unknown_medication_is_refused_by_inventory_record();
    resolving_clears_the_notification();
    escalation_is_delivered_to_dashboards();
    inbox_shown_to_the_logged_in_user();
    withdraw_upload_goes_through_the_router();
    history_is_stored_encrypted_on_disk();
    history_search_reads_the_file();
    daily_report_reads_the_file();
    real_write_failure_becomes_a_verdict();
    corrupted_record_becomes_a_read_failure();
    restart_keeps_the_key_and_starts_a_fresh_trail();
    simulated_failure_switch_still_works();
    std::cout << "integrated_test: " << passed << " tests passed\n";
    return 0;
}
