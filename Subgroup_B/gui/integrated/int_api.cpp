//integrated api

#include "gui/integrated/int_api.hpp"

#include <cctype>
#include <climits>
#include <vector>

#include "gui/json.hpp"
#include "gui/names.hpp"

namespace hb = hms::behaviour;

static const char* const REGISTRY_RESULT[] = {"Ok", "DuplicateId", "NotFound", "InvalidId", "InvalidLocation"};

// ---- Answers ----

static IntResponse answer(bool ok, const std::string& message, const std::string& verdict = "",
                          const std::string& extraKey = "", const std::string& extraJson = "") {
    JsonObject body;
    body.add("ok", ok).add("message", message);
    if (!verdict.empty()) body.add("verdict", verdict);
    if (!extraKey.empty()) body.raw(extraKey, extraJson);
    return {200, body.text()};
}

static IntResponse failWith(int status, const std::string& message) {
    return {status, JsonObject().add("ok", false).add("message", message).text()};
}

// ---- Reading parameters; each returns false if missing or malformed ----

static bool param(const IntRequest& r, const std::string& key, std::string& out) {
    auto found = r.params.find(key);
    if (found == r.params.end()) return false;
    out = found->second;
    return true;
}

static std::string paramOr(const IntRequest& r, const std::string& key, const std::string& fallback) {
    std::string value;
    return param(r, key, value) ? value : fallback;
}

static bool number(const IntRequest& r, const std::string& key, long long& out) {
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

static bool intNumber(const IntRequest& r, const std::string& key, int& out) {
    long long value = 0;
    if (!number(r, key, value) || value < INT_MIN || value > INT_MAX) return false;
    out = static_cast<int>(value);
    return true;
}

// "ADC-3" or "3" gives 3 (under the given key). Zero or anything else is not an ADC.
static bool adcParam(const IntRequest& r, hb::AdcId& out, const std::string& key = "adc") {
    std::string text;
    if (!param(r, key, text)) return false;
    if (text.rfind("ADC-", 0) == 0) text = text.substr(4);
    if (text.empty() || text.size() > 9) return false;
    for (char c : text) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    out = std::stoi(text);
    return out > 0;
}

static const char* const LOGIN_RESULT[] = {"Success", "InvalidCredentials", "AccountLocked"};

// Requests that simulate the world rather than act as a hospital user.
static bool isSimulation(const std::string& p) {
    static const char* const simulation[] = {
        "/api/restart", "/api/scenario", "/api/advance", "/api/advance-to-six", "/api/fail/history",
        "/api/fail/sender", "/api/heartbeat", "/api/silence", "/api/resume", "/api/alarm/raise",
        "/api/stock/withdraw", "/api/stock/deposit", "/api/login", "/api/logout", "/api/message/send"};
    for (const char* s : simulation) {
        if (p == s) return true;
    }
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

// ---- Module data as JSON ----

static std::string entryJson(const IntWorld& w, const hb::HistoryEntry& e) {
    return JsonObject().add("time", e.time).add("timeText", w.timeText(e.time)).add("who", e.who)
        .add("kind", e.kind).add("detail", e.detail).add("adc", e.adc > 0 ? hb::adcName(e.adc) : "")
        .add("medication", e.medication).text();
}

static std::string hospitalAlarm(const IntWorld& w, const std::string& medication) {
    std::string latest = "none";
    for (AlarmId id = 1; w.alarms.find(id).has_value(); id++) {
        AlarmInfo a = *w.alarms.find(id);
        if (a.adc == HospitalStock::HOSPITAL && a.type == AlarmType::HospitalLowStock && a.detail == medication) {
            latest = "#" + std::to_string(id) + " " + nameOf(a.state, names::ALARM_STATE);
        }
    }
    return latest;
}

static std::string traceJson(const MessageTrace& t) {
    JsonArray removed;
    for (const std::string& field : t.removed) removed.add(field);
    return JsonObject().add("raw", t.raw).add("encryptedHex", t.encryptedHex)
        .add("packetBytes", static_cast<long long>(t.packetBytes)).add("decrypted", t.decrypted)
        .add("stripped", t.stripped).raw("removed", removed.text()).add("outcome", t.outcome).text();
}

static const char* windowState(const IntWindow& v, long long now) {
    if (v.cancelled) return "Cancelled";
    if (now < v.start) return "Scheduled";
    if (now < v.end) return "Active";
    return "Completed";
}

// ---- The API ----

IntegratedApi::IntegratedApi(const std::string& dir) : dataDir(dir), current(std::make_unique<IntWorld>(dir)) {}

// A fresh world (fresh audit trail). If the storage can't be opened, the old
// world is kept and the failure is reported.
IntResponse IntegratedApi::restartWorld() {
    try {
        current.reset();
        current = std::make_unique<IntWorld>(dataDir);
        return answer(true, "Restarted: fresh integrated HMS at " + current->timeText(current->clock.now()));
    } catch (const std::exception& e) {
        return failWith(500, std::string("Could not restart: storage unavailable (") + e.what() + ")");
    }
}

IntResponse IntegratedApi::handle(const IntRequest& request) {
    if (request.path == "/api/state" || request.path == "/api/history") {
        if (request.method != "GET") return failWith(405, "use GET");
        return request.path == "/api/state" ? state() : historySearch(request);
    }
    static const char* const actions[] = {
        "/api/restart", "/api/scenario", "/api/login", "/api/logout", "/api/users/add",
        "/api/users/remove", "/api/users/unlock", "/api/advance", "/api/advance-to-six",
        "/api/fail/history", "/api/fail/sender", "/api/report/daily", "/api/report/summary",
        "/api/reserve/set", "/api/alarm/ack", "/api/alarm/resolve", "/api/heartbeat", "/api/silence",
        "/api/resume", "/api/adc/register", "/api/adc/relocate", "/api/adc/remove",
        "/api/maintenance/schedule", "/api/maintenance/cancel", "/api/alarm/raise",
        "/api/stock/withdraw", "/api/stock/deposit", "/api/stock/set", "/api/message/send"};
    bool known = false;
    for (const char* a : actions) known = known || request.path == a;
    if (!known) return failWith(404, "unknown request " + request.path);
    if (request.method != "POST") return failWith(405, "use POST");
    return action(request);
}

IntResponse IntegratedApi::state() {
    IntWorld& w = *current;
    long long now = w.clock.now();
    bool loggedIn = !w.sessionUser.empty() && w.auth.isLoggedIn(w.sessionUser);
    if (!loggedIn && !w.sessionUser.empty()) {   // the session timed out since the last request
        w.note("action", "Session for " + w.sessionUser + " ended (15 minutes with no activity)");
        w.sessionUser.clear();
    }

    JsonArray adcs;
    for (const ADC& adc : w.adcList.all()) {
        auto heard = w.lastHeartbeat.find(adc.id);
        adcs.raw(JsonObject().add("id", adc.id).add("name", hb::adcName(adc.id)).add("location", adc.location)
                     .add("status", nameOf(w.heartbeats.status(adc.id), names::ADC_STATUS))
                     .add("inMaintenance", w.maintenance.isInMaintenance(adc.id, now))
                     .add("silenced", w.silenced.count(adc.id) > 0)
                     .add("lastHeartbeat", heard == w.lastHeartbeat.end() ? "" : w.timeText(heard->second))
                     .text());
    }

    JsonArray alarms;
    for (AlarmId id = 1; w.alarms.find(id).has_value(); id++) {
        AlarmInfo a = *w.alarms.find(id);
        alarms.raw(JsonObject().add("id", id).add("type", nameOf(a.type, names::ALARM_TYPE)).add("detail", a.detail)
                       .add("adc", hb::adcName(a.adc)).add("severity", nameOf(a.severity, names::SEVERITY))
                       .add("state", nameOf(a.state, names::ALARM_STATE))
                       .add("destination", nameOf(a.destination, names::ROLE)).add("escalated", a.escalated)
                       .add("raisedAt", w.timeText(a.raisedAt)).text());
    }

    LowStockRule lowStock;
    JsonArray stock;
    for (const StockItem& item : w.inventory.items()) {
        stock.raw(JsonObject().add("adc", hb::adcName(item.adc)).add("medication", item.medication)
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
    for (const IntWindow& v : w.windows) {
        windows.raw(JsonObject().add("adc", hb::adcName(v.adc)).add("start", v.start).add("startText", w.timeText(v.start))
                        .add("endText", w.timeText(v.end)).add("state", windowState(v, now)).text());
    }

    JsonArray feed;
    for (auto it = w.feed.rbegin(); it != w.feed.rend(); ++it) {
        feed.raw(JsonObject().add("time", w.timeText(it->time)).add("kind", it->kind).add("text", it->text).text());
    }

    JsonArray permissions;
    const hb::Role roles[] = {hb::Role::Pharmacist, hb::Role::Technician, hb::Role::Administrator};
    for (std::size_t a = 0; a < sizeof names::ACTION / sizeof names::ACTION[0]; a++) {
        JsonObject row;
        row.add("action", names::ACTION[a]);
        for (hb::Role role : roles) row.add(nameOf(role, names::ROLE), w.permissions.mayPerform(role, static_cast<hb::Action>(a)));
        permissions.raw(row.text());
    }

    JsonArray components;
    for (const Component& c : w.components()) {
        components.raw(JsonObject().add("name", c.name).add("owner", c.owner).add("real", c.real).add("note", c.note).text());
    }

    JsonObject session;
    session.add("loggedIn", loggedIn);
    if (loggedIn) {
        std::optional<Session> s = w.auth.session(w.sessionUser);
        long long last = 0;
        convert::fromText(s->lastActivity, last);
        session.add("user", w.sessionUser).add("role", UserSettings::roleName(s->role))
               .add("since", s->loginTime).add("timeoutAt", w.timeText(last + Authorization::kSessionTimeoutSeconds));
    }

    JsonObject body;
    body.add("ok", true)
        .raw("clock", JsonObject().add("seconds", now).add("text", w.timeText(now)).text())
        .raw("session", session.text()).raw("components", components.text()).raw("adcs", adcs.text())
        .raw("lastMessage", traceJson(w.lastTrace)).add("queued", static_cast<long long>(w.queued()))
        .raw("storage", JsonObject().add("folder", w.dataDir).add("realFailures", w.history.realFailures)
                            .add("lastError", w.history.lastError).text())
        .raw("failures", JsonObject().add("history", w.historyFailing).add("sender", w.senderFailing).text());
    if (!loggedIn) return {200, body.text()};   // no medication, alarm or user data before login

    // Users, and who receives each severity (Inodh's chart; nobody in a role means administrators).
    JsonArray users;
    for (UserRole role : w.settings.allRoles()) {
        for (const std::string& id : w.users.usersWithRole(role)) {
            users.raw(JsonObject().add("id", id).add("role", UserSettings::roleName(role))
                          .add("locked", w.users.isAccountLocked(id)).text());
        }
    }
    JsonArray recipients;
    for (SwAlarmSeverity severity : {SwAlarmSeverity::Critical, SwAlarmSeverity::High, SwAlarmSeverity::Low}) {
        JsonArray who;
        for (UserRole role : AlarmNotifier::recipientsFor(severity)) {
            std::string list;
            for (const std::string& id : w.users.usersWithRole(role)) list += (list.empty() ? "" : ", ") + id;
            who.raw(JsonObject().add("role", UserSettings::roleName(role))
                        .add("users", list.empty() ? "nobody: goes to administrators" : list).text());
        }
        recipients.raw(JsonObject().add("severity", AlarmNotifier::severityName(severity)).raw("to", who.text()).text());
    }

    JsonArray inbox;   // newest first
    std::vector<Inbox::Message> mine = w.inbox.for_(w.sessionUser);
    for (auto it = mine.rbegin(); it != mine.rend(); ++it) {
        inbox.raw(JsonObject().add("time", w.timeText(it->time)).add("message", it->text).text());
    }
    JsonArray notifications;   // Inodh's notifier's open alarms and their alarm-resolution twins
    for (const AlarmRecord& record : w.swNotifier.pending()) {
        std::optional<AlarmId> twin = w.links.mine(record.id);
        notifications.raw(JsonObject().add("id", record.id).add("code", record.code).add("description", record.description)
                              .add("severity", AlarmNotifier::severityName(record.severity))
                              .add("adc", hb::adcName(convert::fromSwAdc(record.adcId)))
                              .add("alarm", twin ? "#" + std::to_string(*twin) : std::string("none (unknown code)")).text());
    }

    body.raw("users", users.text()).raw("recipients", recipients.text()).raw("inbox", inbox.text())
        .raw("notifications", notifications.text())
        .raw("user", JsonObject().add("name", w.user.name).add("role", nameOf(w.user.role, names::ROLE)).text())
        .raw("alarms", alarms.text()).raw("stock", stock.text())
        .raw("hospital", hospital.text()).raw("windows", windows.text()).raw("feed", feed.text())
        .raw("permissions", permissions.text())
        .add("lastDaily", nameOf(w.lastDaily, names::DAILY))
        .add("reportsSent", static_cast<int>(w.sender.sent.size()));
    return {200, body.text()};
}

IntResponse IntegratedApi::historySearch(const IntRequest& r) {
    IntWorld& w = *current;
    if (!w.touchSession()) return failWith(401, "Please log in");
    long long hours = 24;
    if (r.params.count("hours") && (!number(r, "hours", hours) || hours <= 0 || hours > 24 * 400)) {
        return failWith(400, "hours must be a whole number from 1 to 9600");
    }
    hb::AdcId adc = 0;
    if (!paramOr(r, "adc", "").empty() && !adcParam(r, adc)) return failWith(400, "adc must be like ADC-1 or 1");
    std::string med = paramOr(r, "med", ""), actor = paramOr(r, "actor", ""), kind = paramOr(r, "kind", "");

    long long now = w.clock.now();
    hb::HistoryReadResult read = w.history.read(now - hours * 3600, now + 1);
    if (read.verdict != hb::ReadVerdict::Ok) return answer(false, "History could not be read (StorageFailure)", "StorageFailure");
    JsonArray entries;
    int count = 0;
    for (auto it = read.entries.rbegin(); it != read.entries.rend(); ++it) {
        const hb::HistoryEntry& e = *it;
        if ((adc != 0 && e.adc != adc) || (!med.empty() && e.medication != med)
            || (!actor.empty() && e.who != actor) || (!kind.empty() && e.kind.rfind(kind, 0) != 0)) continue;
        entries.raw(entryJson(w, e));
        count++;
    }
    return answer(true, std::to_string(count) + " history entries", "", "entries", entries.text());
}

IntResponse IntegratedApi::action(const IntRequest& r) {
    IntWorld& w = *current;
    const std::string& p = r.path;
    hb::AdcId adc = 0;
    long long n = 0;
    int amount = 0;

    if (p == "/api/restart") return restartWorld();
    if (p == "/api/scenario") return scenario(paramOr(r, "name", ""));
    if (!isSimulation(p) && !w.touchSession()) return failWith(401, "Please log in (or log in again: the session ended)");

    if (p == "/api/login") {
        std::string id = paramOr(r, "user", "");
        Authorization::LoginResult result = w.login(id, paramOr(r, "password", ""));
        bool ok = result == Authorization::LoginResult::Success;
        return answer(ok, ok ? "Logged in as " + id : Authorization::describe(result), nameOf(result, LOGIN_RESULT));
    }
    if (p == "/api/logout") {
        w.logout();
        return answer(true, "Logged out");
    }
    if (p == "/api/users/add" || p == "/api/users/remove" || p == "/api/users/unlock") {
        Permission needed = p == "/api/users/unlock" ? Permission::UnlockAccounts : Permission::ManageUsers;
        if (!w.auth.authorize(w.sessionUser, needed)) {
            return answer(false, UserSettings::permissionName(needed) + " is not allowed for "
                          + nameOf(w.user.role, names::ROLE) + " (Inodh's user settings)", "NotAuthorized");
        }
        std::string id = paramOr(r, "user", "");
        if (id.empty()) return failWith(400, "give a user id (user)");
        bool ok = false;
        std::string message;
        if (p == "/api/users/add") {
            UserRole role;
            if (!UserSettings::roleFromName(paramOr(r, "role", ""), role)) {
                return failWith(400, "role must be Technician, Pharmacist or Administrator");
            }
            ok = w.users.addUser(id, paramOr(r, "password", ""), role);
            message = ok ? "User " + id + " added (" + UserSettings::roleName(role) + ")"
                         : "User " + id + " not added: the id is taken, or the id or password is empty";
        } else if (p == "/api/users/remove") {
            if (id == w.sessionUser) return answer(false, "You cannot remove the account you are logged in with");
            ok = w.users.removeUser(id);
            message = ok ? "User " + id + " removed" : "No user " + id;
        } else {
            ok = w.users.unlockAccount(id);
            message = ok ? "User " + id + " unlocked" : "No user " + id;
        }
        if (ok) w.note("action", message + " by " + w.sessionUser);
        return answer(ok, message);
    }
    if (p == "/api/advance") {
        if (!number(r, "seconds", n) || n <= 0 || n > IntWorld::MAX_ADVANCE) {
            return failWith(400, "seconds must be a whole number from 1 to " + std::to_string(IntWorld::MAX_ADVANCE));
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
        if (on != "1" && on != "0") return failWith(400, "on must be 1 or 0");
        bool failing = on == "1";
        if (p == "/api/fail/history") w.setHistoryFailing(failing); else w.setSenderFailing(failing);
        std::string what = p == "/api/fail/history" ? "History (read and write)" : "Report sending";
        w.note("action", what + (failing ? " is now FAILING (simulated)" : " is working again"));
        return answer(true, what + (failing ? " is now failing (simulated)" : " is working again"));
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
            return failWith(400, "hours must be a whole number from 1 to 9600");
        }
        SummaryFilters filters{0, paramOr(r, "med", ""), paramOr(r, "actor", ""), w.clock.now() - hours * 3600, w.clock.now()};
        if (!paramOr(r, "adc", "").empty() && !adcParam(r, filters.adc)) return failWith(400, "adc must be like ADC-1 or 1");
        SummaryResult s = w.reports.summary(w.user, filters);
        bool ok = s.verdict == SummaryVerdict::Summarized;
        return answer(ok, ok ? "Summary of " + std::to_string(s.summary.entries.size()) + " entries (request audited)"
                             : std::string("Summary refused (request audited unless AuditFailure)"),
                      nameOf(s.verdict, names::SUMMARY), "report", jsonString(ok ? w.format.formatSummary(s.summary) : ""));
    }
    if (p == "/api/reserve/set") {
        std::string med = paramOr(r, "med", "");
        if (med.empty() || !intNumber(r, "count", amount)) return failWith(400, "give med and a whole-number count");
        ReserveVerdict v = w.hospital.manualReserveChange(w.user, med, amount, paramOr(r, "explanation", ""));
        w.hospital.checkAll();
        return answer(v == ReserveVerdict::Updated, "Reserve " + med + " set to " + std::to_string(amount) + " as " + w.user.name,
                      nameOf(v, names::RESERVE));
    }
    if (p == "/api/alarm/ack" || p == "/api/alarm/resolve") {
        if (!number(r, "id", n) || n <= 0 || n > INT_MAX) return failWith(400, "id must be an alarm number");
        AlarmId id = static_cast<AlarmId>(n);
        if (p == "/api/alarm/ack") {
            AcknowledgeVerdict v = w.alarms.acknowledge(w.user, id);
            return answer(v == AcknowledgeVerdict::Acknowledged, "Acknowledge alarm #" + std::to_string(id) + " as " + w.user.name,
                          nameOf(v, names::ACK));
        }
        ResolveVerdict v = w.alarms.resolve(w.user, id, paramOr(r, "note", ""));
        return answer(v == ResolveVerdict::Resolved, "Resolve alarm #" + std::to_string(id) + " as " + w.user.name,
                      nameOf(v, names::RESOLVE));
    }

    if (p == "/api/message/send") {
        std::string text = paramOr(r, "text", "");
        if (text.empty() || text.size() > 500) return failWith(400, "text must be a message of 1 to 500 characters");
        MessageTrace t = w.sendMessage(text);
        bool ok = t.outcome == "Event" || t.outcome == "Heartbeat";
        return answer(ok, "ADC message routed: " + t.outcome, t.outcome, "trace", traceJson(t));
    }

    // ADC registry (Inodh's ADClist). Who may change it is Inodh's own table.
    if (p == "/api/adc/register" || p == "/api/adc/relocate" || p == "/api/adc/remove") {
        long long id = 0;
        if (!number(r, "id", id) || id < INT_MIN || id > INT_MAX) return failWith(400, "id must be a whole number");
        Permission needed = p == "/api/adc/register" ? Permission::RegisterADC
                          : p == "/api/adc/relocate" ? Permission::RelocateADC : Permission::RemoveADC;
        if (!w.auth.authorize(w.sessionUser, needed)) {
            return answer(false, UserSettings::permissionName(needed) + " is not allowed for "
                          + nameOf(w.user.role, names::ROLE) + " (Inodh's user settings)", "NotAuthorized");
        }
        std::string location = paramOr(r, "location", "");
        ADClist::Result result = p == "/api/adc/register" ? w.registerAdc(static_cast<int>(id), location, w.user.name)
                               : p == "/api/adc/relocate" ? w.relocateAdc(static_cast<int>(id), location, w.user.name)
                               : w.removeAdc(static_cast<int>(id), w.user.name);
        std::string what = p == "/api/adc/register" ? "Register" : p == "/api/adc/relocate" ? "Relocate" : "Remove";
        if (result == ADClist::Result::Ok) w.note("action", what + " ADC-" + std::to_string(id) + " by " + w.user.name);
        return answer(result == ADClist::Result::Ok, what + " ADC-" + std::to_string(id) + ": " + ADClist::describe(result),
                      nameOf(result, REGISTRY_RESULT));
    }

    // Everything below names an ADC.
    if (!adcParam(r, adc)) return failWith(400, "adc must be like ADC-1 or 1");
    std::string name = hb::adcName(adc);

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
    if (p == "/api/maintenance/schedule") {
        long long startMin = 0, lengthMin = 0;
        if (!number(r, "startMin", startMin) || !number(r, "lengthMin", lengthMin)
            || startMin < -1440 || startMin > 14 * 1440 || lengthMin < -1440 || lengthMin > 2 * 1440) {
            return failWith(400, "startMin (minutes from now) and lengthMin must be whole numbers");
        }
        long long start = w.clock.now() + startMin * 60, end = start + lengthMin * 60;
        ScheduleVerdict v = w.maintenance.schedule(w.user, adc, start, end);
        if (v == ScheduleVerdict::Created) w.windows.push_back({adc, start, end, false});
        return answer(v == ScheduleVerdict::Created, "Maintenance for " + name + ", " + w.timeText(start) + " to "
                      + w.timeText(end) + ", as " + w.user.name, nameOf(v, names::SCHEDULE));
    }
    if (p == "/api/maintenance/cancel") {
        if (!number(r, "start", n)) return failWith(400, "start must be the window's start in seconds");
        CancelVerdict v = w.maintenance.cancel(w.user, adc, n);
        if (v == CancelVerdict::Cancelled) {
            for (IntWindow& view : w.windows) {
                if (view.adc == adc && view.start == n && !view.cancelled) { view.cancelled = true; break; }
            }
        }
        return answer(v == CancelVerdict::Cancelled, "Cancel maintenance for " + name + " as " + w.user.name,
                      nameOf(v, names::CANCEL));
    }
    if (p == "/api/alarm/raise") {
        AlarmType type;
        if (!alarmTypeFromName(paramOr(r, "type", ""), type)) return failWith(400, "type must be an alarm type name");
        std::size_t before = w.feed.size();
        w.notifier.raise(adc, type, paramOr(r, "detail", ""));
        return answer(true, "Alarm raised: " + (w.feed.size() > before ? w.feed.back().text : std::string("raised")));
    }

    std::string med = paramOr(r, "med", "");
    if (med.empty()) return failWith(400, "give a medication (med)");
    if (p == "/api/stock/withdraw" || p == "/api/stock/deposit") {
        if (!intNumber(r, "amount", amount)) return failWith(400, "amount must be a whole number");
        bool withdraw = p == "/api/stock/withdraw";
        MessageTrace t = w.upload(adc, withdraw ? "Withdrawal" : "Deposit", med, amount);
        return answer(t.outcome == "Event", std::string(withdraw ? "Withdrawal" : "Deposit") + " upload from " + name
                      + " routed: " + t.outcome, t.outcome, "trace", traceJson(t));
    }
    if (p == "/api/stock/set") {
        if (!intNumber(r, "count", amount)) return failWith(400, "count must be a whole number");
        InventoryVerdict v = w.inventory.manualChange(w.user, adc, med, amount, paramOr(r, "explanation", ""));
        w.hospital.checkAll();
        return answer(v == InventoryVerdict::Updated, "Set " + med + " in " + name + " to " + std::to_string(amount)
                      + " as " + w.user.name, nameOf(v, names::INVENTORY));
    }
    return failWith(404, "unknown request " + p);
}

IntResponse IntegratedApi::scenario(const std::string& name) {
    const char* known[] = {"offline", "maintenance", "lowstock", "discrepancy", "escalation", "daily"};
    bool found = false;
    for (const char* k : known) found = found || name == k;
    if (!found) return failWith(400, "name must be offline, maintenance, lowstock, discrepancy, escalation or daily");

    IntResponse restarted = restartWorld();
    if (restarted.status != 200) return restarted;
    IntWorld& w = *current;
    auto step = [&](const std::string& text) { w.note("scenario", text); };
    std::string summary;
    if (name == "offline") {
        step("Scenario offline: ADC-1 stops sending heartbeats.");
        w.silenced.insert(1);
        w.advance(180);
        step("After 3 missed 60 s periods ADC-1 is Offline and AdcOffline is raised.");
        summary = "ADC-1 went offline after 180 s";
    } else if (name == "maintenance") {
        w.login("tara", "tara-demo");
        step("Scenario maintenance: tara (Technician) gives ADC-2 a 10-minute window, then it goes silent.");
        w.maintenance.schedule(w.user, 2, w.clock.now(), w.clock.now() + 600);
        w.windows.push_back({2, w.clock.now(), w.clock.now() + 600, false});
        w.silenced.insert(2);
        w.advance(300);
        step("ADC-2 is Offline but no alarm is raised during maintenance.");
        w.advance(330);
        step("The window has ended and ADC-2 is still silent, so AdcOffline is raised.");
        summary = "suppressed during maintenance, raised when the window ended";
    } else if (name == "lowstock") {
        w.login("priya", "priya-demo");
        step("Scenario low stock: priya (Pharmacist) is logged in.");
        w.upload(1, "Withdrawal", "Morphine", 20);
        step("ADC-1 sent a withdrawal of 20 Morphine through the message path: 20 of 100 is below 25%, "
             "so InventoryLow is raised and delivered to the pharmacist.");
        w.hospital.manualReserveChange(w.user, "Insulin", 20, "stock take found 20");
        w.hospital.checkAll();
        step("Insulin reserve set to 20: the hospital total (50 of 600) is low, so HospitalLowStock is raised.");
        summary = "one ADC low; then the hospital low while every ADC is fine";
    } else if (name == "discrepancy") {
        step("Scenario discrepancy: ADC-2 reports withdrawing 70 Morphine, but the HMS count is 60.");
        w.upload(2, "Withdrawal", "Morphine", 70);
        step("The router sets the count to 0 and raises DISCREPANCY; alarm resolution tracks it as "
             "InventoryDiscrepancy, and InventoryLow follows.");
        summary = "over-withdrawal recorded as a discrepancy";
    } else if (name == "escalation") {
        step("Scenario escalation: ADC-1 goes silent and nobody acknowledges the alarm.");
        w.silenced.insert(1);
        w.advance(180);
        w.advance(120);
        step("2 minutes later the Critical alarm is still unacknowledged, so it is escalated.");
        summary = "Critical alarm escalated after 2 minutes";
    } else {
        step("Scenario daily report: ADC-2 goes silent at 05:40, then the clock moves to 06:00.");
        w.silenced.insert(2);
        long long now = w.clock.now();
        long long six = now / 86400 * 86400 + 6 * 3600;
        if (six <= now) six += 86400;
        w.advance(six - now);
        step("The daily report was sent at 06:00 and flags ADC-2 for missing heartbeats.");
        summary = "daily report sent at 06:00";
    }
    return answer(true, "Scenario " + name + ": " + summary);
}
