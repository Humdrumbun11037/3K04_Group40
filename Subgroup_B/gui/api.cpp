//gui api

#include "gui/api.hpp"

#include <cctype>
#include <climits>
#include <vector>

#include "gui/json.hpp"
#include "gui/names.hpp"

using namespace hms::behaviour;

// ---- Answers ----

static ApiResponse answer(bool ok, const std::string& message, const std::string& verdict = "",
                          const std::string& extraKey = "", const std::string& extraJson = "") {
    JsonObject body;
    body.add("ok", ok).add("message", message);
    if (!verdict.empty()) body.add("verdict", verdict);
    if (!extraKey.empty()) body.raw(extraKey, extraJson);
    return {200, body.text()};
}

static ApiResponse badRequest(const std::string& message) {
    return {400, JsonObject().add("ok", false).add("message", message).text()};
}

// ---- Reading parameters. Each returns false if the parameter is missing or malformed. ----

static bool param(const ApiRequest& r, const std::string& key, std::string& out) {
    auto found = r.params.find(key);
    if (found == r.params.end()) return false;
    out = found->second;
    return true;
}

static std::string paramOr(const ApiRequest& r, const std::string& key, const std::string& fallback) {
    std::string value;
    return param(r, key, value) ? value : fallback;
}

// A whole number, optionally negative, that fits in a long long.
static bool number(const ApiRequest& r, const std::string& key, long long& out) {
    std::string text;
    if (!param(r, key, text) || text.empty() || text.size() > 18) return false;
    std::size_t i = (text[0] == '-') ? 1 : 0;
    if (i == text.size()) return false;
    for (; i < text.size(); i++) {
        if (!std::isdigit(static_cast<unsigned char>(text[i]))) return false;
    }
    out = std::stoll(text);
    return true;
}

static bool intNumber(const ApiRequest& r, const std::string& key, int& out) {
    long long value = 0;
    if (!number(r, key, value) || value < INT_MIN || value > INT_MAX) return false;
    out = static_cast<int>(value);
    return true;
}

// "ADC-3" or "3" gives 3. Zero or anything else is not an ADC.
static bool adcParam(const ApiRequest& r, AdcId& out) {
    std::string text;
    if (!param(r, "adc", text)) return false;
    if (text.rfind("ADC-", 0) == 0) text = text.substr(4);
    if (text.empty() || text.size() > 9) return false;
    for (char c : text) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    out = std::stoi(text);
    return out > 0;
}

static bool roleFromName(const std::string& text, User& user) {
    if (text == "technician")    { user = {"Tara", Role::Technician};    return true; }
    if (text == "pharmacist")    { user = {"Priya", Role::Pharmacist};   return true; }
    if (text == "administrator") { user = {"Alex", Role::Administrator}; return true; }
    return false;
}

static bool alarmTypeFromName(const std::string& text, AlarmType& type) {
    for (std::size_t i = 0; i < sizeof names::ALARM_TYPE / sizeof names::ALARM_TYPE[0]; i++) {
        if (text == names::ALARM_TYPE[i]) {
            type = static_cast<AlarmType>(i);
            return true;
        }
    }
    return false;
}

// ---- Turning module data into JSON ----

static std::string entryJson(const GuiWorld& w, const HistoryEntry& e) {
    return JsonObject().add("time", e.time).add("timeText", w.timeText(e.time)).add("who", e.who)
        .add("kind", e.kind).add("detail", e.detail).add("adc", e.adc > 0 ? adcName(e.adc) : "")
        .add("medication", e.medication).text();
}

// The latest HospitalLowStock alarm for a medication: "#id State", or "none".
static std::string hospitalAlarm(const GuiWorld& w, const std::string& medication) {
    std::string latest = "none";
    for (AlarmId id = 1; w.alarms.find(id).has_value(); id++) {
        AlarmInfo a = *w.alarms.find(id);
        if (a.adc == HospitalStock::HOSPITAL && a.type == AlarmType::HospitalLowStock && a.detail == medication) {
            latest = "#" + std::to_string(id) + " " + nameOf(a.state, names::ALARM_STATE);
        }
    }
    return latest;
}

static const char* windowState(const WindowView& v, long long now) {
    if (v.cancelled) return "Cancelled";
    if (now < v.start) return "Scheduled";
    if (now < v.end) return "Active";
    return "Completed";
}

// ---- The API ----

HmsApi::HmsApi() : current(std::make_unique<GuiWorld>()) {}

ApiResponse HmsApi::handle(const ApiRequest& request) {
    if (request.path == "/api/state" || request.path == "/api/history") {
        if (request.method != "GET") return {405, JsonObject().add("ok", false).add("message", "use GET").text()};
        return request.path == "/api/state" ? state() : historySearch(request);
    }
    static const char* const actions[] = {
        "/api/restart", "/api/scenario", "/api/role", "/api/advance", "/api/advance-to-six",
        "/api/fail/history", "/api/fail/sender", "/api/report/daily", "/api/report/summary",
        "/api/reserve/set", "/api/alarm/ack", "/api/alarm/resolve", "/api/heartbeat", "/api/silence",
        "/api/resume", "/api/adc/add", "/api/adc/decommission", "/api/maintenance/schedule",
        "/api/maintenance/cancel", "/api/alarm/raise", "/api/stock/withdraw", "/api/stock/deposit",
        "/api/stock/set"};
    bool known = false;
    for (const char* a : actions) known = known || request.path == a;
    if (!known) {
        return {404, JsonObject().add("ok", false).add("message", "unknown request " + request.path).text()};
    }
    if (request.method != "POST") return {405, JsonObject().add("ok", false).add("message", "use POST").text()};
    return action(request);
}

ApiResponse HmsApi::state() {
    GuiWorld& w = *current;
    long long now = w.clock.now();

    JsonArray adcs;
    for (const AdcId& adc : w.registry.activeAdcs()) {
        auto heard = w.lastHeartbeat.find(adc);
        adcs.raw(JsonObject().add("id", adc).add("name", adcName(adc))
                     .add("status", nameOf(w.heartbeats.status(adc), names::ADC_STATUS))
                     .add("inMaintenance", w.maintenance.isInMaintenance(adc, now))
                     .add("silenced", w.silenced.count(adc) > 0)
                     .add("lastHeartbeat", heard == w.lastHeartbeat.end() ? "" : w.timeText(heard->second))
                     .text());
    }

    JsonArray alarms;
    for (AlarmId id = 1; w.alarms.find(id).has_value(); id++) {
        AlarmInfo a = *w.alarms.find(id);
        alarms.raw(JsonObject().add("id", id).add("type", nameOf(a.type, names::ALARM_TYPE)).add("detail", a.detail)
                       .add("adc", adcName(a.adc)).add("severity", nameOf(a.severity, names::SEVERITY))
                       .add("state", nameOf(a.state, names::ALARM_STATE))
                       .add("destination", nameOf(a.destination, names::ROLE)).add("escalated", a.escalated)
                       .add("raisedAt", w.timeText(a.raisedAt)).text());
    }

    LowStockRule lowStock;
    JsonArray stock;
    for (const StockItem& item : w.inventory.items()) {
        stock.raw(JsonObject().add("adc", adcName(item.adc)).add("medication", item.medication)
                      .add("count", item.count).add("capacity", item.capacity)
                      .add("low", lowStock.check(item.count, item.capacity) == LowStockVerdict::Low).text());
    }

    JsonArray hospital;
    for (const HospitalStockLevel& m : w.hospital.all()) {
        hospital.raw(JsonObject().add("medication", m.medication).add("reserveCount", m.reserveCount)
                         .add("reserveCapacity", m.reserveCapacity).add("adcCount", m.adcCount)
                         .add("adcCapacity", m.adcCapacity).add("totalCount", m.totalCount)
                         .add("totalCapacity", m.totalCapacity).add("verdict", nameOf(m.verdict, names::HOSPITAL))
                         .add("alarm", hospitalAlarm(w, m.medication)).text());
    }

    JsonArray windows;
    for (const WindowView& v : w.windows) {
        windows.raw(JsonObject().add("adc", adcName(v.adc)).add("start", v.start).add("startText", w.timeText(v.start))
                        .add("endText", w.timeText(v.end)).add("state", windowState(v, now)).text());
    }

    JsonArray feed;   // newest first
    for (auto it = w.feed.rbegin(); it != w.feed.rend(); ++it) {
        feed.raw(JsonObject().add("time", w.timeText(it->time)).add("kind", it->kind).add("text", it->text).text());
    }

    JsonArray permissions;
    const Role roles[] = {Role::Pharmacist, Role::Technician, Role::Administrator};
    for (std::size_t a = 0; a < sizeof names::ACTION / sizeof names::ACTION[0]; a++) {
        JsonObject row;
        row.add("action", names::ACTION[a]);
        for (Role role : roles) row.add(nameOf(role, names::ROLE), w.permissions.mayPerform(role, static_cast<Action>(a)));
        permissions.raw(row.text());
    }

    JsonObject body;
    body.add("ok", true)
        .raw("clock", JsonObject().add("seconds", now).add("text", w.timeText(now)).text())
        .raw("user", JsonObject().add("name", w.user.name).add("role", nameOf(w.user.role, names::ROLE)).text())
        .raw("adcs", adcs.text()).raw("alarms", alarms.text()).raw("stock", stock.text())
        .raw("hospital", hospital.text()).raw("windows", windows.text()).raw("feed", feed.text())
        .raw("permissions", permissions.text())
        .raw("failures", JsonObject().add("history", w.historyFailing).add("sender", w.senderFailing).text())
        .add("lastDaily", nameOf(w.lastDaily, names::DAILY))
        .add("reportsSent", static_cast<int>(w.sender.sent.size()));
    return {200, body.text()};
}

// History search reads the fake history store directly, as the records-searching
// module would; unlike an activity summary it writes no audit entry.
ApiResponse HmsApi::historySearch(const ApiRequest& r) {
    GuiWorld& w = *current;
    long long hours = 24;
    if (r.params.count("hours") && (!number(r, "hours", hours) || hours <= 0 || hours > 24 * 400)) {
        return badRequest("hours must be a whole number from 1 to 9600");
    }
    AdcId adc = 0;
    if (!paramOr(r, "adc", "").empty() && !adcParam(r, adc)) return badRequest("adc must be like ADC-1 or 1");
    std::string med = paramOr(r, "med", ""), actor = paramOr(r, "actor", ""), kind = paramOr(r, "kind", "");

    long long now = w.clock.now();
    HistoryReadResult read = w.history.read(now - hours * 3600, now + 1);
    if (read.verdict != ReadVerdict::Ok) return answer(false, "History could not be read (StorageFailure)", "StorageFailure");
    JsonArray entries;
    int count = 0;
    for (auto it = read.entries.rbegin(); it != read.entries.rend(); ++it) {   // newest first
        const HistoryEntry& e = *it;
        if ((adc != 0 && e.adc != adc) || (!med.empty() && e.medication != med)
            || (!actor.empty() && e.who != actor) || (!kind.empty() && e.kind.rfind(kind, 0) != 0)) continue;
        entries.raw(entryJson(w, e));
        count++;
    }
    return answer(true, std::to_string(count) + " history entries", "", "entries", entries.text());
}

ApiResponse HmsApi::action(const ApiRequest& r) {
    GuiWorld& w = *current;
    const std::string& p = r.path;
    AdcId adc = 0;
    long long n = 0;
    int amount = 0;
    std::string text;

    if (p == "/api/restart") {
        current = std::make_unique<GuiWorld>();
        return answer(true, "Restarted: fresh HMS at " + current->timeText(current->clock.now()));
    }
    if (p == "/api/scenario") return scenario(paramOr(r, "name", ""));

    if (p == "/api/role") {
        if (!roleFromName(paramOr(r, "role", ""), w.user)) return badRequest("role must be pharmacist, technician or administrator");
        w.note("action", "User is now " + w.user.name + " (" + nameOf(w.user.role, names::ROLE) + ")");
        return answer(true, "You are now " + w.user.name + " (" + nameOf(w.user.role, names::ROLE) + ")");
    }
    if (p == "/api/advance") {
        if (!number(r, "seconds", n) || n <= 0 || n > GuiWorld::MAX_ADVANCE) {
            return badRequest("seconds must be a whole number from 1 to " + std::to_string(GuiWorld::MAX_ADVANCE));
        }
        w.advance(n);
        return answer(true, "Clock moved " + std::to_string(n) + " s to " + w.timeText(w.clock.now()));
    }
    if (p == "/api/advance-to-six") {
        long long now = w.clock.now();
        long long six = now / 86400 * 86400 + 6 * 3600;
        if (six <= now) six += 86400;
        w.advance(six - now);
        return answer(true, "Clock moved to " + w.timeText(w.clock.now()));
    }
    if (p == "/api/fail/history" || p == "/api/fail/sender") {
        std::string on = paramOr(r, "on", "");
        if (on != "1" && on != "0") return badRequest("on must be 1 or 0");
        bool failing = on == "1";
        if (p == "/api/fail/history") w.setHistoryFailing(failing); else w.setSenderFailing(failing);
        std::string what = p == "/api/fail/history" ? "History (read and write)" : "Report sending";
        w.note("action", what + (failing ? " is now FAILING" : " is working again"));
        return answer(true, what + (failing ? " is now failing" : " is working again"));
    }
    if (p == "/api/report/daily") {
        DailyVerdict v = w.runDaily();
        std::string report = w.sender.sent.empty() ? "" : w.format.formatDaily(w.sender.sent.back());
        bool ok = v == DailyVerdict::Sent || v == DailyVerdict::AlreadyDone;
        std::string message = v == DailyVerdict::Sent ? "Daily report sent"
                            : v == DailyVerdict::AlreadyDone ? "Today's report was already sent (shown below)"
                            : v == DailyVerdict::TooEarly ? "Too early: the daily report is due at 06:00"
                            : "Daily report failed; it is logged and retried on the next tick";
        return answer(ok, message, nameOf(v, names::DAILY), "report", jsonString(ok ? report : ""));
    }
    if (p == "/api/report/summary") {
        long long hours = 24;
        if (r.params.count("hours") && (!number(r, "hours", hours) || hours <= 0 || hours > 24 * 400)) {
            return badRequest("hours must be a whole number from 1 to 9600");
        }
        SummaryFilters filters{0, paramOr(r, "med", ""), paramOr(r, "actor", ""),
                               w.clock.now() - hours * 3600, w.clock.now()};
        if (!paramOr(r, "adc", "").empty() && !adcParam(r, filters.adc)) return badRequest("adc must be like ADC-1 or 1");
        SummaryResult s = w.reports.summary(w.user, filters);
        bool ok = s.verdict == SummaryVerdict::Summarized;
        return answer(ok, ok ? "Summary of " + std::to_string(s.summary.entries.size()) + " entries (request audited)"
                             : std::string("Summary refused (request audited unless AuditFailure)"),
                      nameOf(s.verdict, names::SUMMARY), "report", jsonString(ok ? w.format.formatSummary(s.summary) : ""));
    }
    if (p == "/api/reserve/set") {
        std::string med = paramOr(r, "med", "");
        if (med.empty() || !intNumber(r, "count", amount)) return badRequest("give med and a whole-number count");
        ReserveVerdict v = w.hospital.manualReserveChange(w.user, med, amount, paramOr(r, "explanation", ""));
        w.hospital.checkAll();
        return answer(v == ReserveVerdict::Updated, "Reserve " + med + " set to " + std::to_string(amount)
                      + " as " + w.user.name, nameOf(v, names::RESERVE));
    }
    if (p == "/api/alarm/ack" || p == "/api/alarm/resolve") {
        if (!number(r, "id", n) || n <= 0 || n > INT_MAX) return badRequest("id must be an alarm number");
        AlarmId id = static_cast<AlarmId>(n);
        if (p == "/api/alarm/ack") {
            AcknowledgeVerdict v = w.alarms.acknowledge(w.user, id);
            return answer(v == AcknowledgeVerdict::Acknowledged, "Acknowledge alarm #" + std::to_string(id)
                          + " as " + w.user.name, nameOf(v, names::ACK));
        }
        ResolveVerdict v = w.alarms.resolve(w.user, id, paramOr(r, "note", ""));
        return answer(v == ResolveVerdict::Resolved, "Resolve alarm #" + std::to_string(id) + " as " + w.user.name,
                      nameOf(v, names::RESOLVE));
    }

    // Everything below names an ADC.
    if (!adcParam(r, adc)) return badRequest("adc must be like ADC-1 or 1");
    std::string name = adcName(adc);

    if (p == "/api/heartbeat") {
        if (!w.registry.isActive(adc)) return answer(false, name + " is not registered", "NotMonitored");
        w.sendHeartbeat(adc);
        w.periodicChecks();
        return answer(true, "Heartbeat sent from " + name);
    }
    if (p == "/api/silence" || p == "/api/resume") {
        if (p == "/api/silence") w.silenced.insert(adc); else w.silenced.erase(adc);
        std::string what = name + (p == "/api/silence" ? " stopped sending heartbeats" : " resumed sending heartbeats");
        w.note("action", what);
        return answer(true, what);
    }
    if (p == "/api/adc/add") {
        if (w.registry.isActive(adc)) return answer(false, name + " is already registered");
        w.addAdc(adc);
        w.note("action", name + " registered and monitored");
        return answer(true, name + " registered and monitored");
    }
    if (p == "/api/adc/decommission") {
        if (!w.registry.isActive(adc)) return answer(false, name + " is not registered");
        w.decommission(adc);
        w.note("action", name + " decommissioned: no longer monitored");
        return answer(true, name + " decommissioned: no longer monitored");
    }
    if (p == "/api/maintenance/schedule") {
        long long startMin = 0, lengthMin = 0;
        if (!number(r, "startMin", startMin) || !number(r, "lengthMin", lengthMin)
            || startMin < -1440 || startMin > 14 * 1440 || lengthMin < -1440 || lengthMin > 2 * 1440) {
            return badRequest("startMin (minutes from now) and lengthMin must be whole numbers");
        }
        long long start = w.clock.now() + startMin * 60, end = start + lengthMin * 60;
        ScheduleVerdict v = w.maintenance.schedule(w.user, adc, start, end);
        if (v == ScheduleVerdict::Created) w.windows.push_back({adc, start, end, false});
        return answer(v == ScheduleVerdict::Created, "Maintenance for " + name + ", " + w.timeText(start) + " to "
                      + w.timeText(end) + ", as " + w.user.name, nameOf(v, names::SCHEDULE));
    }
    if (p == "/api/maintenance/cancel") {
        if (!number(r, "start", n)) return badRequest("start must be the window's start in seconds");
        CancelVerdict v = w.maintenance.cancel(w.user, adc, n);
        if (v == CancelVerdict::Cancelled) {
            for (WindowView& view : w.windows) {
                if (view.adc == adc && view.start == n && !view.cancelled) { view.cancelled = true; break; }
            }
        }
        return answer(v == CancelVerdict::Cancelled, "Cancel maintenance for " + name + " as " + w.user.name,
                      nameOf(v, names::CANCEL));
    }
    if (p == "/api/alarm/raise") {
        AlarmType type;
        if (!alarmTypeFromName(paramOr(r, "type", ""), type)) return badRequest("type must be an alarm type name");
        std::size_t before = w.feed.size();
        w.notifier.raise(adc, type, paramOr(r, "detail", ""));
        std::string result = w.feed.size() > before ? w.feed.back().text : "raised";
        return answer(true, "Alarm raised: " + result);
    }

    // Stock changes name an ADC and a medication.
    std::string med = paramOr(r, "med", "");
    if (med.empty()) return badRequest("give a medication (med)");
    if (p == "/api/stock/withdraw" || p == "/api/stock/deposit") {
        if (!intNumber(r, "amount", amount)) return badRequest("amount must be a whole number");
        bool withdraw = p == "/api/stock/withdraw";
        InventoryVerdict v = withdraw ? w.inventory.withdraw(adc, med, amount) : w.inventory.deposit(adc, med, amount);
        w.hospital.checkAll();
        bool ok = v == InventoryVerdict::Updated || v == InventoryVerdict::DiscrepancyRecorded;
        return answer(ok, std::string(withdraw ? "Withdraw " : "Deposit ") + std::to_string(amount) + " " + med
                      + (withdraw ? " from " : " into ") + name, nameOf(v, names::INVENTORY));
    }
    if (p == "/api/stock/set") {
        if (!intNumber(r, "count", amount)) return badRequest("count must be a whole number");
        InventoryVerdict v = w.inventory.manualChange(w.user, adc, med, amount, paramOr(r, "explanation", ""));
        w.hospital.checkAll();
        return answer(v == InventoryVerdict::Updated, "Set " + med + " in " + name + " to " + std::to_string(amount)
                      + " as " + w.user.name, nameOf(v, names::INVENTORY));
    }
    return {404, JsonObject().add("ok", false).add("message", "unknown request " + p).text()};
}

// Each scenario starts from a fresh world, runs a few steps through the real
// modules, and leaves the result on screen to explore.
ApiResponse HmsApi::scenario(const std::string& name) {
    const char* known[] = {"offline", "maintenance", "lowstock", "discrepancy", "escalation", "daily"};
    bool found = false;
    for (const char* k : known) found = found || name == k;
    if (!found) return badRequest("name must be offline, maintenance, lowstock, discrepancy, escalation or daily");

    current = std::make_unique<GuiWorld>();
    GuiWorld& w = *current;
    auto step = [&](const std::string& text) { w.note("scenario", text); };
    std::string summary;

    if (name == "offline") {
        step("Scenario offline: ADC-1 stops sending heartbeats.");
        w.silenced.insert(1);
        w.advance(180);
        step("After 3 missed 60 s periods ADC-1 is Offline and AdcOffline (Critical) is raised. "
             "Try resolving alarm #1 without a note, then with one.");
        summary = "ADC-1 went offline after 180 s; alarm #1 raised";
    } else if (name == "maintenance") {
        step("Scenario maintenance: ADC-2 gets a 10-minute maintenance window, then goes silent.");
        w.maintenance.schedule(w.user, 2, w.clock.now(), w.clock.now() + 600);
        w.windows.push_back({2, w.clock.now(), w.clock.now() + 600, false});
        w.silenced.insert(2);
        w.advance(300);
        step("ADC-2 is Offline but no alarm is raised during maintenance.");
        w.advance(330);
        step("The window has ended and ADC-2 is still silent, so AdcOffline is raised.");
        summary = "suppressed during maintenance, raised when the window ended";
    } else if (name == "lowstock") {
        w.user = {"Priya", Role::Pharmacist};
        step("Scenario low stock: Priya (Pharmacist) is the user.");
        w.inventory.withdraw(1, "Morphine", 20);
        w.hospital.checkAll();
        step("ADC-1 withdrew 20 Morphine: 20 of 100 is below 25%, so InventoryLow (High) is raised for ADC-1.");
        w.hospital.manualReserveChange(w.user, "Insulin", 20, "stock take found 20");
        w.hospital.checkAll();
        step("The Insulin reserve was set to 20: every ADC is fine, but the hospital total (50 of 600) is low, "
             "so HospitalLowStock is raised for the whole hospital.");
        summary = "one ADC low; then the hospital low while every ADC is fine";
    } else if (name == "discrepancy") {
        step("Scenario discrepancy: ADC-2 reports withdrawing 70 Morphine, but the HMS count is 60.");
        w.inventory.withdraw(2, "Morphine", 70);
        w.hospital.checkAll();
        step("The ADC has already dispensed, so it is recorded: the count goes to 0, InventoryDiscrepancy "
             "(High) is raised, and InventoryLow follows.");
        summary = "over-withdrawal recorded as a discrepancy";
    } else if (name == "escalation") {
        step("Scenario escalation: ADC-1 goes silent and nobody acknowledges the alarm.");
        w.silenced.insert(1);
        w.advance(180);
        step("AdcOffline (Critical) is raised and sent to the Technician.");
        w.advance(120);
        step("2 minutes later it is still unacknowledged, so it is escalated to the Administrator.");
        summary = "Critical alarm escalated after 2 minutes";
    } else {
        step("Scenario daily report: ADC-2 goes silent at 05:40, then the clock moves to 06:00.");
        w.silenced.insert(2);
        long long now = w.clock.now();
        long long six = now / 86400 * 86400 + 6 * 3600;
        if (six <= now) six += 86400;
        w.advance(six - now);
        step("The daily report was sent at 06:00 and flags ADC-2 for missing heartbeats. "
             "Open Reports to see it.");
        summary = "daily report sent at 06:00";
    }
    return answer(true, "Scenario " + name + ": " + summary);
}
