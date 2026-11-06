//integrated world

#include "gui/integrated/int_world.hpp"

#include <exception>
#include <filesystem>

#include "gui/names.hpp"

namespace hb = hms::behaviour;

static const long long DAY_SECONDS = 24 * 60 * 60;
static const std::size_t FEED_LIMIT = 300;
static const char* const OUTCOME[] = {"Heartbeat", "Event", "UnknownAdc", "Malformed", "NotAnEvent"};

// Removes the previous run's audit files (not the key), so each world starts
// with an empty history; creates the folder if needed.
bool IntWorld::startFresh(const std::string& dir) {
    std::filesystem::create_directories(dir);
    std::filesystem::remove(dir + "/active.log");
    std::filesystem::remove(dir + "/archive.log");
    return true;
}

// Throws (from Inodh's key storage or Matthew's storage) only if the data
// folder can't be used at all; the server reports that and stops.
IntWorld::IntWorld(const std::string& dir)
    : dataDir(dir),
      freshData(startFresh(dir)),
      key(loadOrCreateEncryptionKey(dir + "/key.bin")),
      encryption(EncryptionManager::defaultStrategy(), key),
      trail(dir + "/active.log", dir + "/archive.log", encryption) {
    registerAdc(1, "Ward 3 West", "setup");
    registerAdc(2, "ICU", "setup");
    for (const hb::AdcId& adc : registry.activeAdcs()) {
        for (long long t = START_TIME - DAY_SECONDS + HEARTBEAT_SECONDS; t <= START_TIME; t += HEARTBEAT_SECONDS) {
            heartbeatLog.record(adc, t);
        }
    }
    inventory.addItem(1, "Morphine", 40, 100);
    inventory.addItem(1, "Insulin", 30, 100);
    inventory.addItem(2, "Morphine", 60, 100);
    hospital.addReserve("Morphine", 300, 400);
    hospital.addReserve("Insulin", 130, 500);
    hospital.addReserve("Heparin", 150, 200);
    for (const DemoAccount& account : DEMO_ACCOUNTS) users.addUser(account.id, account.password, account.role);
    note("action", "Integrated HMS started: ADC-1 (Ward 3 West) and ADC-2 (ICU) registered; please log in");
}

// ---- Registry ----

ADClist::Result IntWorld::registerAdc(int id, const std::string& location, const std::string& byUser) {
    ADClist::Result r = adcList.addADC(id, location, byUser);
    if (r == ADClist::Result::Ok) {
        heartbeats.monitor(id);
        shownStatus[id] = AdcStatus::Online;
        lastHeartbeat[id] = clock.now();
    }
    return r;
}

ADClist::Result IntWorld::relocateAdc(int id, const std::string& location, const std::string& byUser) {
    return adcList.relocateADC(id, location, byUser);
}

ADClist::Result IntWorld::removeAdc(int id, const std::string& byUser) {
    ADClist::Result r = adcList.removeADC(id, byUser);
    if (r == ADClist::Result::Ok) {
        heartbeats.stopMonitoring(id);
        shownStatus.erase(id);
        silenced.erase(id);
        lastHeartbeat.erase(id);
        outbox.erase(id);
    }
    return r;
}

// ---- Login ----

Authorization::LoginResult IntWorld::login(const std::string& id, const std::string& password) {
    Authorization::LoginResult result = auth.login(id, password);
    if (result == Authorization::LoginResult::Success) {
        if (!sessionUser.empty() && sessionUser != id) auth.logout(sessionUser);
        sessionUser = id;
        touchSession();
        note("action", id + " logged in (" + UserSettings::roleName(users.getUserRole(id)) + ")");
    } else {
        note("action", "Login failed for " + id + ": " + Authorization::describe(result));
    }
    return result;
}

void IntWorld::logout() {
    if (sessionUser.empty()) return;
    auth.logout(sessionUser);
    note("action", sessionUser + " logged out");
    sessionUser.clear();
    user = {"", hb::Role::Technician};
}

// Inodh's authorize() both checks the session (15-minute timeout, account
// removed or locked) and renews it; every role holds ViewADCs, so this asks
// only "is the session live?".
bool IntWorld::touchSession() {
    if (sessionUser.empty()) return false;
    if (!auth.authorize(sessionUser, Permission::ViewADCs)) {
        note("action", "Session for " + sessionUser + " ended (15 minutes with no activity, or account locked or removed)");
        sessionUser.clear();
        user = {"", hb::Role::Technician};
        return false;
    }
    user = {sessionUser, convert::toRole(users.getUserRole(sessionUser))};
    return true;
}

// ---- Message path ----

// The simulated ADC encrypts its message with the shared key, as a real ADC would.
bool IntWorld::enqueue(int sender, const std::string& plaintext) {
    std::deque<std::vector<uint8_t>>& queue = outbox[sender];
    if (queue.size() >= QUEUE_LIMIT || plaintext.empty()) return false;
    queue.push_back(encryption.encryptForADC(std::vector<uint8_t>(plaintext.begin(), plaintext.end())));
    return true;
}

// One packet from one ADC's queue through Inodh's router. The router catches
// decryption errors itself; anything else it throws is caught here, so one
// bad message can't stop the HMS.
std::string IntWorld::routeOne(int sender) {
    auto found = outbox.find(sender);
    if (found == outbox.end() || found->second.empty()) return "nothing queued";
    std::vector<uint8_t> packet = found->second.front();
    found->second.pop_front();
    std::string outcome;
    try {
        outcome = nameOf(router.routePacket(packet), OUTCOME);
    } catch (const std::exception& e) {
        outcome = std::string("RouterError: ") + e.what();
        note("message", "Inodh's router threw an exception on a message from ADC " + std::to_string(sender)
                        + " (" + e.what() + "); the message was dropped and the HMS kept running");
    }
    links.sync(swNotifier);   // alarms the router raised itself
    hospital.checkAll();
    return outcome;
}

// At most one packet per ADC per tick.
void IntWorld::routeAll() {
    std::vector<int> senders;
    for (const auto& [sender, queue] : outbox) {
        if (!queue.empty()) senders.push_back(sender);
    }
    for (int sender : senders) routeOne(sender);
}

std::size_t IntWorld::queued() const {
    std::size_t n = 0;
    for (const auto& [sender, queue] : outbox) n += queue.size();
    return n;
}

MessageTrace IntWorld::sendMessage(const std::string& raw) {
    MessageTrace trace;
    trace.raw = raw;
    // The sender is the ADC id at the start of the message (0 if unreadable).
    int sender = 0;
    std::size_t bar = raw.find('|');
    std::string idText = raw.substr(0, bar);
    if (!idText.empty() && idText.size() <= 9 && idText.find_first_not_of("0123456789") == std::string::npos) {
        sender = std::stoi(idText);
    }
    if (!enqueue(sender, raw)) {
        trace.outcome = raw.empty() ? "Empty message" : "ADC outbox full; try again after advancing the clock";
        lastTrace = trace;
        return trace;
    }
    const std::vector<uint8_t>& packet = outbox[sender].back();
    trace.packetBytes = packet.size();
    static const char hex[] = "0123456789ABCDEF";
    for (std::size_t i = 0; i < packet.size() && i < 24; i++) {
        trace.encryptedHex += hex[packet[i] >> 4];
        trace.encryptedHex += hex[packet[i] & 0x0F];
    }
    try {
        std::vector<uint8_t> plain = encryption.decryptADCMessage(packet);
        trace.decrypted.assign(plain.begin(), plain.end());
    } catch (const std::exception& e) {
        trace.decrypted = std::string("(could not decrypt: ") + e.what() + ")";
    }
    trace.stripped = stripper.strip(trace.decrypted);
    std::string field;
    for (std::size_t i = 0; i <= trace.decrypted.size(); i++) {   // fields the stripper removed
        if (i == trace.decrypted.size() || trace.decrypted[i] == '|') {
            if (stripper.isPatientField(field)) trace.removed.push_back(field);
            field.clear();
        } else {
            field += trace.decrypted[i];
        }
    }
    // This ADC gets one routing turn now. Normally its queue held only this
    // message; if earlier ones were waiting, they go first, one per tick.
    bool alone = outbox[sender].size() == 1;
    std::string outcome = routeOne(sender);
    trace.outcome = alone ? outcome : "Queued behind earlier messages; routed on later ticks";
    note("message", "ADC message \"" + trace.stripped + "\" (" + std::to_string(trace.removed.size())
                    + " patient field(s) stripped): " + outcome);
    lastTrace = trace;
    return trace;
}

void IntWorld::sendHeartbeat(const hb::AdcId& adc) {
    if (enqueue(adc, std::to_string(adc) + "|Heartbeat")) routeOne(adc);
}

MessageTrace IntWorld::upload(const hb::AdcId& adc, const std::string& keyword, const std::string& medication, int amount) {
    return sendMessage(std::to_string(adc) + "|" + keyword + "|" + medication + "|" + std::to_string(amount) + "|ADC-USER");
}

// ---- Time ----

// Bounded: at most MAX_ADVANCE / TICK_SECONDS steps, each with a fixed amount of
// work (one packet per ADC queue, then the periodic checks).
void IntWorld::advance(long long seconds) {
    if (seconds > MAX_ADVANCE) seconds = MAX_ADVANCE;
    while (seconds > 0) {
        long long step = seconds < TICK_SECONDS ? seconds : TICK_SECONDS;
        long long minuteBefore = clock.now() / HEARTBEAT_SECONDS;
        clock.advance(step);
        seconds -= step;
        if (clock.now() / HEARTBEAT_SECONDS != minuteBefore) {
            for (const hb::AdcId& adc : registry.activeAdcs()) {
                if (silenced.count(adc) == 0) enqueue(adc, std::to_string(adc) + "|Heartbeat");
            }
        }
        routeAll();
        periodicChecks();
    }
}

void IntWorld::periodicChecks() {
    heartbeats.checkAll();
    hospital.checkAll();
    links.sync(swNotifier);
    noteStatusChanges();
    // Andrew's escalation; delivered to the new role's users the way Inodh's
    // notification delivers (nobody in the role: administrators).
    for (const Escalation& e : alarms.escalateDue()) {
        std::string text = "[ESCALATED] alarm #" + std::to_string(e.id) + " now " + nameOf(e.severity, names::SEVERITY)
                         + ", to " + nameOf(e.destination, names::ROLE);
        std::vector<std::string> to = users.usersWithRole(convert::toUserRole(e.destination));
        if (to.empty()) to = users.usersWithRole(UserRole::Administrator);
        for (const std::string& id : to) inbox.deliver(id, text);
        note("escalation", "Alarm #" + std::to_string(e.id) + " escalated: now to "
                           + nameOf(e.destination, names::ROLE) + ", " + nameOf(e.severity, names::SEVERITY));
    }
    DailyVerdict v = reports.generateDaily(clock.now());
    if (v == DailyVerdict::Sent || (v != lastDaily && v != DailyVerdict::AlreadyDone && v != DailyVerdict::TooEarly)) {
        note("report", std::string("Daily report: ") + nameOf(v, names::DAILY)
                       + (v == DailyVerdict::Sent ? "" : " (logged; retried on the next tick)"));
    }
    lastDaily = v;
}

DailyVerdict IntWorld::runDaily() {
    DailyVerdict v = reports.generateDaily(clock.now());
    note("report", std::string("Daily report requested: ") + nameOf(v, names::DAILY));
    lastDaily = v;
    return v;
}

void IntWorld::setHistoryFailing(bool failing) {
    historyFailing = failing;
    history.failWrites(failing);
    history.failReads(failing);
}

void IntWorld::setSenderFailing(bool failing) {
    senderFailing = failing;
    sender.failSends(failing);
}

void IntWorld::note(const std::string& kind, const std::string& text) {
    feed.push_back({clock.now(), kind, text});
    if (feed.size() > FEED_LIMIT) feed.erase(feed.begin(), feed.end() - FEED_LIMIT);
}

void IntWorld::noteStatusChanges() {
    for (const hb::AdcId& adc : registry.activeAdcs()) {
        AdcStatus status = heartbeats.status(adc);
        if (shownStatus[adc] == status) continue;
        shownStatus[adc] = status;
        bool inMaintenance = maintenance.isInMaintenance(adc, clock.now());
        note("status", hb::adcName(adc) + " is now " + nameOf(status, names::ADC_STATUS)
                       + (inMaintenance ? " (in maintenance: no alarm)" : ""));
    }
}

std::vector<Component> IntWorld::components() const {
    return {
        {"Alarm severity, alarm resolution, report types, report formatting", "Andrew", true, ""},
        {"Inventory record, low stock rule, hospital stock", "Andrew", true, ""},
        {"ADC heartbeats, maintenance", "Andrew", true, ""},
        {"ADC registry", "Inodh", true, "through RegistryAdapter"},
        {"Login, 4-attempt lockout, sessions (15 min)", "Inodh", true, "authorization + user registry"},
        {"User accounts and roles", "Inodh", true, "user registry + user settings"},
        {"Encryption of ADC messages", "Inodh", true, "ChaCha20-Poly1305"},
        {"Patient info stripping", "Inodh", true, ""},
        {"Event parsing and message routing", "Inodh", true, "one packet per ADC per tick; exceptions caught"},
        {"Alarm notification (delivery to dashboards)", "Inodh", true, "his 6-minute escalation is not used"},
        {"Event logging (event recorder)", "Inodh", true, "events stored in the shared history"},
        {"Permission table for Andrew's modules", "team", false, "the agreed table (FakePermissions)"},
        {"History storage (audit trail, records searching)", "Matthew", true,
         "encrypted files gui/data/active.log and archive.log; history search and the daily report read them"},
        {"Encryption key storage", "Inodh", true, "gui/data/key.bin, kept between runs"},
        {"Archiving after 13 months", "Matthew", false, "not exercised: the demo spans days, not months"},
        {"Clock", "Matthew", false, "simulated so the demo can move time"},
        {"ADCs and the link to them", "Subgroup A / Matthew", false,
         "simulated ADCs send encrypted packets; Matthew's comms link wraps a stub, so it is not used"},
        {"Dashboards", "Matthew", false, "each user's messages shown on this page"},
        {"Heartbeat counts for the daily report", "team", false, "counted in memory as heartbeats arrive"},
        {"Report delivery", "team", false, "kept in memory"},
    };
}
