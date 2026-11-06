// Tests for the GUI's API (gui/api.cpp): each request reaches the real
// behaviour module, reports its verdict, and changes the world as expected.
// No HTTP is involved, so this runs without cpp-httplib.

#include <cassert>
#include <iostream>
#include <string>

#include "gui/api.hpp"
#include "gui/json.hpp"

static int passed = 0;

static ApiResponse get(HmsApi& api, const std::string& path, std::map<std::string, std::string> params = {}) {
    return api.handle({"GET", path, params});
}

static ApiResponse post(HmsApi& api, const std::string& path, std::map<std::string, std::string> params = {}) {
    return api.handle({"POST", path, params});
}

static bool has(const ApiResponse& r, const std::string& text) {
    return r.body.find(text) != std::string::npos;
}

static void json_escapes_quotes_backslashes_and_newlines() {
    assert(jsonString("a\"b\\c\nd") == "\"a\\\"b\\\\c\\nd\"");
    assert(jsonString(std::string(1, '\x01')) == "\"\\u0001\"");
    assert(JsonObject().add("n", 3).add("t", true).add("s", "x").text() == "{\"n\":3,\"t\":true,\"s\":\"x\"}");
    passed++;
}

static void state_shows_starting_world() {
    HmsApi api;
    ApiResponse r = get(api, "/api/state");
    assert(r.status == 200);
    assert(has(r, "\"text\":\"2026-11-02 05:40:00\""));
    assert(has(r, "\"name\":\"ADC-1\"") && has(r, "\"name\":\"ADC-2\""));
    assert(has(r, "\"role\":\"Technician\""));
    assert(has(r, "\"medication\":\"Heparin\""));        // reserve-only medication
    assert(has(r, "\"action\":\"ScheduleMaintenance\""));
    passed++;
}

static void unknown_request_is_404_and_wrong_method_is_405() {
    HmsApi api;
    assert(post(api, "/api/nothing").status == 404);
    assert(post(api, "/api/state").status == 405);
    assert(get(api, "/api/advance", {{"seconds", "10"}}).status == 405);
    assert(get(api, "/index.html").status == 404);
    passed++;
}

static void bad_parameters_are_400_and_change_nothing() {
    HmsApi api;
    assert(post(api, "/api/advance", {{"seconds", "abc"}}).status == 400);
    assert(post(api, "/api/advance", {{"seconds", "0"}}).status == 400);
    assert(post(api, "/api/advance", {{"seconds", "999999999"}}).status == 400);
    assert(post(api, "/api/stock/withdraw", {{"adc", "X"}, {"med", "Morphine"}, {"amount", "1"}}).status == 400);
    assert(post(api, "/api/role", {{"role", "nurse"}}).status == 400);
    assert(api.world().clock.now() == GuiWorld::START_TIME);
    passed++;
}

static void role_switch_changes_the_user() {
    HmsApi api;
    assert(has(post(api, "/api/role", {{"role", "pharmacist"}}), "Priya (Pharmacist)"));
    assert(api.world().user.role == hms::behaviour::Role::Pharmacist);
    assert(has(get(api, "/api/state"), "\"role\":\"Pharmacist\""));
    passed++;
}

static void silent_adc_goes_offline_and_alarm_is_raised() {
    HmsApi api;
    post(api, "/api/silence", {{"adc", "ADC-1"}});
    post(api, "/api/advance", {{"seconds", "180"}});
    ApiResponse s = get(api, "/api/state");
    assert(has(s, "\"status\":\"Offline\""));
    assert(has(s, "\"type\":\"AdcOffline\"") && has(s, "\"severity\":\"Critical\""));
    passed++;
}

static void resolve_needs_a_note() {
    HmsApi api;
    post(api, "/api/scenario", {{"name", "offline"}});
    ApiResponse blank = post(api, "/api/alarm/resolve", {{"id", "1"}, {"note", "  "}});
    assert(has(blank, "\"ok\":false") && has(blank, "\"verdict\":\"NoExplanation\""));
    ApiResponse noted = post(api, "/api/alarm/resolve", {{"id", "1"}, {"note", "replaced cable"}});
    assert(has(noted, "\"ok\":true") && has(noted, "\"verdict\":\"Resolved\""));
    assert(has(get(api, "/api/history", {{"kind", "AlarmResolved"}}), "replaced cable"));
    passed++;
}

static void acknowledge_reports_verdict() {
    HmsApi api;
    post(api, "/api/scenario", {{"name", "offline"}});
    assert(has(post(api, "/api/alarm/ack", {{"id", "1"}}), "\"verdict\":\"Acknowledged\""));
    assert(has(post(api, "/api/alarm/ack", {{"id", "1"}}), "\"verdict\":\"NotRaised\""));
    assert(has(post(api, "/api/alarm/ack", {{"id", "99"}}), "\"verdict\":\"UnknownAlarm\""));
    passed++;
}

static void permission_table_is_followed() {
    HmsApi api;   // Tara, Technician
    ApiResponse r = post(api, "/api/stock/set", {{"adc", "1"}, {"med", "Morphine"}, {"count", "50"},
                                                 {"explanation", "count"}});
    assert(has(r, "\"verdict\":\"NotAuthorized\""));
    post(api, "/api/role", {{"role", "pharmacist"}});
    r = post(api, "/api/stock/set", {{"adc", "1"}, {"med", "Morphine"}, {"count", "50"}, {"explanation", "count"}});
    assert(has(r, "\"verdict\":\"Updated\""));
    assert(has(post(api, "/api/maintenance/schedule", {{"adc", "1"}, {"startMin", "0"}, {"lengthMin", "30"}}),
               "\"verdict\":\"NotAuthorized\""));
    passed++;
}

static void manual_change_needs_explanation() {
    HmsApi api;
    post(api, "/api/role", {{"role", "administrator"}});
    assert(has(post(api, "/api/stock/set", {{"adc", "1"}, {"med", "Morphine"}, {"count", "50"}}),
               "\"verdict\":\"NoExplanation\""));
    assert(has(post(api, "/api/reserve/set", {{"med", "Insulin"}, {"count", "50"}}), "\"verdict\":\"NoExplanation\""));
    passed++;
}

static void withdraw_and_deposit_change_counts() {
    HmsApi api;
    assert(has(post(api, "/api/stock/withdraw", {{"adc", "1"}, {"med", "Morphine"}, {"amount", "5"}}),
               "\"verdict\":\"Updated\""));
    assert(*api.world().inventory.count(1, "Morphine") == 35);
    assert(has(post(api, "/api/stock/deposit", {{"adc", "1"}, {"med", "Morphine"}, {"amount", "10"}}),
               "\"verdict\":\"Updated\""));
    assert(*api.world().inventory.count(1, "Morphine") == 45);
    assert(has(post(api, "/api/stock/withdraw", {{"adc", "1"}, {"med", "Aspirin"}, {"amount", "1"}}),
               "\"verdict\":\"UnknownItem\""));
    passed++;
}

static void history_failure_blocks_the_change() {
    HmsApi api;
    post(api, "/api/fail/history", {{"on", "1"}});
    assert(has(post(api, "/api/stock/withdraw", {{"adc", "1"}, {"med", "Morphine"}, {"amount", "5"}}),
               "\"verdict\":\"StorageFailure\""));
    assert(*api.world().inventory.count(1, "Morphine") == 40);
    assert(has(get(api, "/api/history"), "\"ok\":false"));
    post(api, "/api/fail/history", {{"on", "0"}});
    assert(has(get(api, "/api/history"), "\"ok\":true"));
    passed++;
}

static void maintenance_schedule_and_cancel() {
    HmsApi api;   // Technician may schedule
    ApiResponse r = post(api, "/api/maintenance/schedule", {{"adc", "2"}, {"startMin", "5"}, {"lengthMin", "30"}});
    assert(has(r, "\"verdict\":\"Created\""));
    assert(has(get(api, "/api/state"), "\"state\":\"Scheduled\""));
    std::string start = std::to_string(GuiWorld::START_TIME + 300);
    assert(has(post(api, "/api/maintenance/cancel", {{"adc", "2"}, {"start", start}}), "\"verdict\":\"Cancelled\""));
    assert(has(get(api, "/api/state"), "\"state\":\"Cancelled\""));
    assert(has(post(api, "/api/maintenance/schedule", {{"adc", "2"}, {"startMin", "0"}, {"lengthMin", "800"}}),
               "\"verdict\":\"WindowTooLong\""));
    passed++;
}

static void every_scenario_runs() {
    HmsApi api;
    const char* scenarios[] = {"offline", "maintenance", "lowstock", "discrepancy", "escalation", "daily"};
    for (const char* name : scenarios) {
        ApiResponse r = post(api, "/api/scenario", {{"name", name}});
        assert(r.status == 200 && has(r, "\"ok\":true"));
    }
    assert(post(api, "/api/scenario", {{"name", "nope"}}).status == 400);
    passed++;
}

static void maintenance_scenario_suppresses_then_raises() {
    HmsApi api;
    post(api, "/api/scenario", {{"name", "maintenance"}});
    ApiResponse s = get(api, "/api/state");
    assert(has(s, "\"type\":\"AdcOffline\"") && has(s, "\"adc\":\"ADC-2\""));
    assert(has(s, "in maintenance: no alarm"));
    passed++;
}

static void lowstock_scenario_raises_adc_and_hospital_alarms() {
    HmsApi api;
    post(api, "/api/scenario", {{"name", "lowstock"}});
    ApiResponse s = get(api, "/api/state");
    assert(has(s, "\"type\":\"InventoryLow\"") && has(s, "\"type\":\"HospitalLowStock\""));
    assert(has(s, "\"adc\":\"Hospital\""));
    passed++;
}

static void discrepancy_scenario_records_and_alarms() {
    HmsApi api;
    post(api, "/api/scenario", {{"name", "discrepancy"}});
    assert(*api.world().inventory.count(2, "Morphine") == 0);
    assert(has(get(api, "/api/state"), "\"type\":\"InventoryDiscrepancy\""));
    passed++;
}

static void escalation_scenario_reroutes_to_administrator() {
    HmsApi api;
    post(api, "/api/scenario", {{"name", "escalation"}});
    ApiResponse s = get(api, "/api/state");
    assert(has(s, "\"escalated\":true") && has(s, "\"destination\":\"Administrator\""));
    passed++;
}

static void daily_scenario_sends_report_flagging_silent_adc() {
    HmsApi api;
    post(api, "/api/scenario", {{"name", "daily"}});
    assert(api.world().sender.sent.size() == 1);
    ApiResponse r = post(api, "/api/report/daily");
    assert(has(r, "\"verdict\":\"AlreadyDone\"") && has(r, "ADC-2: "));
    assert(r.body.find("ADC-1: ") == std::string::npos);
    passed++;
}

static void daily_report_too_early_before_six() {
    HmsApi api;
    assert(has(post(api, "/api/report/daily"), "\"verdict\":\"TooEarly\""));
    post(api, "/api/advance-to-six");
    assert(api.world().timeText(api.world().clock.now()) == "2026-11-02 06:00:00");
    assert(api.world().sender.sent.size() == 1);
    passed++;
}

static void summary_follows_permissions_and_is_audited() {
    HmsApi api;   // Technician may not
    assert(has(post(api, "/api/report/summary", {{"hours", "1"}}), "\"verdict\":\"NotAuthorized\""));
    post(api, "/api/role", {{"role", "pharmacist"}});
    ApiResponse r = post(api, "/api/report/summary", {{"hours", "1"}, {"adc", "1"}});
    assert(has(r, "\"verdict\":\"Summarized\"") && has(r, "ACTIVITY SUMMARY"));
    assert(has(get(api, "/api/history", {{"kind", "SummaryRequested"}}), "NotAuthorized"));
    passed++;
}

static void restart_gives_a_fresh_world() {
    HmsApi api;
    post(api, "/api/advance", {{"seconds", "600"}});
    post(api, "/api/role", {{"role", "administrator"}});
    post(api, "/api/restart");
    assert(api.world().clock.now() == GuiWorld::START_TIME);
    assert(api.world().user.role == hms::behaviour::Role::Technician);
    passed++;
}

static void adc_add_heartbeat_and_decommission() {
    HmsApi api;
    assert(has(post(api, "/api/adc/add", {{"adc", "ADC-3"}}), "\"ok\":true"));
    assert(has(post(api, "/api/adc/add", {{"adc", "3"}}), "\"ok\":false"));
    assert(has(post(api, "/api/heartbeat", {{"adc", "3"}}), "\"ok\":true"));
    assert(has(post(api, "/api/adc/decommission", {{"adc", "3"}}), "\"ok\":true"));
    assert(has(post(api, "/api/heartbeat", {{"adc", "3"}}), "\"ok\":false"));
    post(api, "/api/advance", {{"seconds", "600"}});
    assert(!has(get(api, "/api/state"), "\"name\":\"ADC-3\""));
    passed++;
}

static void raise_test_alarm_and_unknown_type() {
    HmsApi api;
    assert(has(post(api, "/api/alarm/raise", {{"adc", "1"}, {"type", "UnauthorizedAccess"}}), "Critical, to Administrator"));
    assert(post(api, "/api/alarm/raise", {{"adc", "1"}, {"type", "Fire"}}).status == 400);
    passed++;
}

int main() {
    json_escapes_quotes_backslashes_and_newlines();
    state_shows_starting_world();
    unknown_request_is_404_and_wrong_method_is_405();
    bad_parameters_are_400_and_change_nothing();
    role_switch_changes_the_user();
    silent_adc_goes_offline_and_alarm_is_raised();
    resolve_needs_a_note();
    acknowledge_reports_verdict();
    permission_table_is_followed();
    manual_change_needs_explanation();
    withdraw_and_deposit_change_counts();
    history_failure_blocks_the_change();
    maintenance_schedule_and_cancel();
    every_scenario_runs();
    maintenance_scenario_suppresses_then_raises();
    lowstock_scenario_raises_adc_and_hospital_alarms();
    discrepancy_scenario_records_and_alarms();
    escalation_scenario_reroutes_to_administrator();
    daily_scenario_sends_report_flagging_silent_adc();
    daily_report_too_early_before_six();
    summary_follows_permissions_and_is_audited();
    restart_gives_a_fresh_world();
    adc_add_heartbeat_and_decommission();
    raise_test_alarm_and_unknown_type();
    std::cout << "api_test: " << passed << " tests passed\n";
    return 0;
}
