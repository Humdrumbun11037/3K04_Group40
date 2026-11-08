//live ADC

#include "hms_side/live_adc.hpp"

#include <cctype>
#include <exception>

#include "adc_side/adc_output_table.hpp"
#include "gui/json.hpp"

namespace hb = hms::behaviour;

static const char* const OUTCOME[] = {"Heartbeat", "Event", "UnknownAdc", "Malformed", "NotAnEvent"};

static IntResponse refusal(const std::string& message) {
    return {409, "{\"ok\":false,\"message\":\"" + message + "\"}"};
}

// Subgroup A's starting stock (SW1_InventoryDatabase.cpp), for a cabinet the
// HMS holds no stock for. Subgroup A has no capacities, so these are ours.
void LiveAdc::stockIfEmpty(IntWorld& w, int adc) {
    for (const StockItem& item : w.inventory.items()) {
        if (item.adc == adc) return;
    }
    w.inventory.addItem(adc, "Tylenol", 100, 200);
    w.inventory.addItem(adc, "Morphine", 50, 100);
    w.hospital.addReserve("Tylenol", 300, 400);   // AlreadyExists after the first cabinet: fine
    w.note("action", hb::adcName(adc) + " stocked with Subgroup A's starting stock: Tylenol 100/200, Morphine 50/100");
}

void LiveAdc::setup(IntWorld& w) {
    if (w.adcList.exists(ADC_ID)) return;
    w.registerAdc(ADC_ID, "Ward 5", "integration");
    stockIfEmpty(w, ADC_ID);
    w.note("action", "ADC-3 (Ward 5) registered for the Subgroup A cabinet program. Every ADC is simulated "
                     "until a cabinet program picks it; then only that program's heartbeats count");

    for (const Account& a : ACCOUNTS) {
        w.users.addUser(a.id, a.password, a.role);
        if (w.sessionUser == a.replaces) w.login(a.id, a.password);   // e.g. a scenario logged in as tara
        w.users.removeUser(a.replaces);
    }
    w.note("action", "Accounts: pharm_bob (Pharmacist), tech_tom (Technician), admin_alice (Administrator), "
                     "the same as on the ADC");
}

std::string LiveAdc::correctPage(const std::string& html) {
    std::size_t at = html.find(OLD_LOGIN_HINT);
    if (at == std::string::npos) return html;
    return html.substr(0, at) + LOGIN_HINT + html.substr(at + std::string(OLD_LOGIN_HINT).size());
}

// The ADC id at the start of a decrypted message, if it is a registered ADC; else 0.
static hb::AdcId senderOf(const IntWorld& w, const std::string& message) {
    std::string id = message.substr(0, message.find('|'));
    if (id.empty() || id.size() > 9) return 0;
    for (char c : id) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return 0;
    }
    int adc = std::stoi(id);
    return w.adcList.exists(adc) ? adc : 0;
}

std::string LiveAdc::receive(IntWorld& w, const std::vector<uint8_t>& packet, long long nowMs, const std::string& wallNow) {
    lastPacketMs = nowMs;
    std::string shown;
    bool decrypted = false;
    try {
        std::vector<uint8_t> plain = w.encryption.decryptADCMessage(packet);
        shown = w.stripper.strip(std::string(plain.begin(), plain.end()));
        decrypted = true;
    } catch (const std::exception&) {
        shown = "(could not be decrypted)";
    }
    std::string outcome;
    try {
        outcome = OUTCOME[static_cast<int>(w.router.routePacket(packet))];
    } catch (const std::exception& e) {
        outcome = std::string("RouterError: ") + e.what();
    }

    // Unreadable: decrypted but wrongly formed, or not decryptable at all. One
    // alarm per ADC and cause; repeats join it while it is unresolved.
    if (outcome == "Malformed" || outcome == "NotAnEvent" || outcome.rfind("RouterError", 0) == 0) {
        hb::AdcId adc = decrypted ? senderOf(w, shown) : 0;
        w.notifier.raise(adc, AlarmType::IncorrectMessageFormat,
                         decrypted ? "ADC link: message in the wrong format"
                                   : "ADC link: packet could not be decrypted (wrong key or tampered)");
    }
    w.links.sync(w.swNotifier);   // alarms the router (or this function) raised
    w.hospital.checkAll();

    // The ADC's own time for the message, and how late it arrived.
    std::string late;
    std::string adcTime = adc_output::stampOf(shown);
    long long happened = 0, now = 0;
    if (!adcTime.empty() && timeutil::toSeconds(adcTime, happened) && timeutil::toSeconds(wallNow, now)
        && now - happened > LATE_SECONDS) {
        std::string delay = std::to_string(now - happened) + " s";
        late = " (happened at " + adcTime + " ADC time, arrived " + delay + " later: it waited while the link was down)";
        hb::AdcId adc = senderOf(w, shown);
        std::string kind = shown.substr(shown.find('|') + 1);
        kind = kind.substr(0, kind.find('|'));
        w.history.record({"AdcLateMessage", hb::adcName(adc), kind + " from " + hb::adcName(adc) + " happened at "
                          + adcTime + " ADC local time and arrived " + delay + " later", w.clock.now(), adc, ""});
    }
    if (outcome != "Heartbeat") w.note("message", "Live ADC link: \"" + shown + "\" -> " + outcome + late);
    return outcome;
}

bool LiveAdc::live(long long nowMs) const {
    return !cut && lastPacketMs >= 0 && nowMs - lastPacketMs < LIVE_MS;
}

std::optional<IntResponse> LiveAdc::refuse(const IntRequest& r, long long nowMs) const {
    // Moving the simulated clock while a cabinet is sending would make it miss
    // heartbeats it never had the chance to send, and go Offline falsely.
    if ((r.path == "/api/advance" || r.path == "/api/advance-to-six") && live(nowMs)) {
        return refusal("A real ADC link is live, so the clock runs in real time. "
                       "Cut the link (type cut in the hms_live window) before advancing.");
    }
    // Fake heartbeats for a real cabinet would hide a dropped link.
    auto adc = r.params.find("adc");
    if ((r.path == "/api/resume" || r.path == "/api/heartbeat") && adc != r.params.end()) {
        std::string id = adc->second.rfind("ADC-", 0) == 0 ? adc->second.substr(4) : adc->second;
        for (int a : real) {
            if (id == std::to_string(a)) {
                return refusal(hb::adcName(a) + " is driven by the Subgroup A cabinet program: only its own heartbeats count.");
            }
        }
    }
    return std::nullopt;
}

void LiveAdc::after(IntegratedApi& api, const IntRequest& r, IntResponse& response) {
    if (r.path == "/api/restart" || r.path == "/api/scenario") {
        setup(api.world());
        // A new world: the cabinet programs still drive their ADCs, so those
        // stay real (and get no simulated heartbeats) if they are registered.
        std::set<int> before;
        before.swap(real);
        for (int adc : before) {
            if (api.world().adcList.exists(adc)) makeReal(api.world(), adc);
        }
    }
    if (r.path == "/api/state" && response.status == 200) response.body = correctComponents(response.body);
}

void LiveAdc::makeReal(IntWorld& w, int adc) {
    if (!real.insert(adc).second) return;
    w.silenced.insert(adc);   // the simulator stops sending heartbeats for it
    w.note("action", hb::adcName(adc) + " is now driven by the Subgroup A cabinet program: only its heartbeats count");
}

std::string LiveAdc::cabinets(const IntWorld& w) const {
    std::string out;
    for (const ADC& adc : w.adcList.all()) {
        out += std::to_string(adc.id) + "|" + adc_output::field(adc.location) + "|"
               + (real.count(adc.id) ? "real" : "simulated") + "\n";
    }
    return out;
}

std::string LiveAdc::claim(IntWorld& w, const std::string& request) {
    // "<id>|Claim|<previous>"
    std::size_t bar = request.find('|');
    std::size_t bar2 = request.find('|', bar == std::string::npos ? 0 : bar + 1);
    if (bar == std::string::npos || bar2 == std::string::npos || request.substr(bar + 1, bar2 - bar - 1) != "Claim") {
        return "ERR|not a claim";
    }
    int id = senderOf(w, request.substr(0, bar) + "|");
    if (id == 0) return "ERR|ADC-" + request.substr(0, bar) + " is not registered on the HMS";
    int previous = senderOf(w, request.substr(bar2 + 1) + "|");
    if (previous != 0 && previous != id && real.erase(previous) > 0) {
        w.silenced.erase(previous);   // back to the simulator
        w.note("action", hb::adcName(previous) + " released by the cabinet program: simulated again");
    }
    stockIfEmpty(w, id);
    makeReal(w, id);
    std::string stock;
    for (const StockItem& item : w.inventory.items()) {
        if (item.adc != id) continue;
        stock += (stock.empty() ? "" : ";") + adc_output::field(item.medication) + "=" + std::to_string(item.count);
    }
    return "OK|" + adc_output::field(w.adcList.find(id)->location) + "|" + stock;
}

std::string LiveAdc::correctComponents(const std::string& body) const {
    std::size_t at = body.find(SIMULATED_ADCS_ENTRY);
    if (at == std::string::npos) return body;
    std::string names;
    for (int a : real) names += (names.empty() ? "" : ", ") + hb::adcName(a);
    std::string realEntry = JsonObject().add("name", "Real cabinets: " + (names.empty() ? std::string("none connected yet") : names))
        .add("owner", "Subgroup A / integration").add("real", true)
        .add("note", "Subgroup A's cabinet program (adc_hms_link), one per window: encrypted packets over HTTP to "
                     "127.0.0.1:9040; Matthew's comms link is not used").text();
    std::string simulated = JsonObject().add("name", "Other ADCs and their link")
        .add("owner", "Subgroup A / Matthew").add("real", false)
        .add("note", "simulated ADCs send encrypted packets; Matthew's comms link wraps a stub, so it is not used").text();
    return body.substr(0, at) + realEntry + "," + simulated + body.substr(at + std::string(SIMULATED_ADCS_ENTRY).size());
}
