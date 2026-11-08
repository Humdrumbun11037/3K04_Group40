//live ADC

/*
Hides: how the real Subgroup A cabinet program is joined to Subgroup B's
       integrated HMS (Subgroup_B/gui/integrated, used unchanged): which ADCs
       it is driving at the moment (a "real" ADC gets no simulated heartbeats),
       the cabinet list and stock it is given when it picks a cabinet (demo
       only), the standard accounts, how its encrypted packets reach Inodh's
       router, when the link counts as live, and which dashboard requests must
       be refused or corrected while it is

Interface
takes: encrypted packets and cabinet requests from the ADC endpoint; dashboard
       requests (before and after the integrated API handles them); the real
       time in ms
sends: the router's outcome for each packet; the cabinet list and a cabinet's
       stock (as plain text, for the endpoint to encrypt); a refusal for a request
       that would fake or break a real ADC's data; notes in the activity feed
uses: IntWorld's public parts (registry, inventory, hospital stock, router,
      encryption, alarm links, users, feed); IntegratedApi
*/
#pragma once

#include "gui/integrated/sw_names.hpp"

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "gui/integrated/int_api.hpp"

class LiveAdc {
public:
    static constexpr int ADC_ID = 3;   // registered for the real cabinet at start; any registered ADC can be picked
    static constexpr long long LIVE_MS = 30000;   // a packet in the last 30 s: the link is live

    // The team's standard accounts, the same names and passwords as on the ADC
    // (Subgroup_A/src/SW0_UserDatabase.cpp). The HMS has no Nurse role. Each one
    // replaces one of Subgroup B's demo accounts (IntWorld::DEMO_ACCOUNTS).
    struct Account { const char* id; const char* password; UserRole role; const char* replaces; };
    static constexpr Account ACCOUNTS[] = {
        {"pharm_bob", "hash456", UserRole::Pharmacist, "priya"},
        {"tech_tom", "hash789", UserRole::Technician, "tara"},
        {"admin_alice", "hash000", UserRole::Administrator, "alex"},
    };

    // Subgroup B's login hint on the page (gui/integrated/web/index.html), and ours.
    static constexpr const char* OLD_LOGIN_HINT =
        "Demo accounts: tara / tara-demo (Technician), priya / priya-demo (Pharmacist), alex / alex-demo (Administrator).";
    static constexpr const char* LOGIN_HINT =
        "Accounts: pharm_bob / hash456 (Pharmacist), tech_tom / hash789 (Technician), admin_alice / hash000 (Administrator).";

    // Registers ADC-3 (Ward 5) with Subgroup A's starting stock and puts in the
    // standard accounts (a session of a replaced account moves to its
    // replacement). Does nothing if ADC-3 is already registered, so it can
    // follow every restart. ADC-3 is simulated like ADC-1 and ADC-2 until a
    // cabinet program picks it.
    static void setup(IntWorld& world);

    // Subgroup B's page with LOGIN_HINT in place of OLD_LOGIN_HINT; unchanged if it isn't there.
    static std::string correctPage(const std::string& html);

    static constexpr long long LATE_SECONDS = 5;   // ADC time this far behind wallNow: recorded as late

    // Subgroup B's Components entry for the ADCs, exactly as /api/state writes
    // it (IntWorld::components). tests/live_adc_test fails if it changes.
    static constexpr const char* SIMULATED_ADCS_ENTRY =
        R"({"name":"ADCs and the link to them","owner":"Subgroup A / Matthew","real":false,)"
        R"("note":"simulated ADCs send encrypted packets; Matthew's comms link wraps a stub, so it is not used"})";

    // One packet from the link through Inodh's router, as IntWorld routes its
    // own simulated packets. Never throws. Returns the router's outcome.
    // A packet the router rejects as unreadable raises IncorrectMessageFormat
    // (Critical), as HMS D2 section 3.1 requires; the router itself only drops it.
    // wallNow is this computer's local time ("YYYY-MM-DD HH:MM:SS"); a message
    // stamped by the ADC (ADC_TIME) more than LATE_SECONDS earlier is noted and
    // recorded in history with its real time.
    std::string receive(IntWorld& world, const std::vector<uint8_t>& packet, long long nowMs,
                        const std::string& wallNow = "");

    // ---- Demo cabinet selection (ADC_HMS_INTERFACE.md section 7) ----
    // Every registered ADC, one per line: "<id>|<location>|real" or "...|simulated".
    std::string cabinets(const IntWorld& world) const;
    // "<id>|Claim|<previous id or 0>": the cabinet program now drives <id>
    // (the only way an ADC becomes real). <previous> goes back to the simulator. A cabinet the HMS holds no stock
    // for gets Subgroup A's starting stock first. Answers
    // "OK|<location>|<med>=<count>;..." or "ERR|<reason>".
    std::string claim(IntWorld& world, const std::string& request);

    // ADCs the cabinet program is driving now.
    const std::set<int>& realAdcs() const { return real; }

    bool live(long long nowMs) const;

    // A refusal if the request must not run now (see live_adc.cpp).
    std::optional<IntResponse> refuse(const IntRequest& request, long long nowMs) const;

    // Puts ADC-3 back after a restart or a scenario (both build a new world;
    // the ADCs the cabinet programs drive stay real), and corrects the
    // Components list in /api/state.
    void after(IntegratedApi& api, const IntRequest& request, IntResponse& response);

    // The /api/state body with SIMULATED_ADCS_ENTRY replaced by an entry for the
    // real ADCs and one for the simulated rest; unchanged if the entry isn't there.
    std::string correctComponents(const std::string& stateBody) const;

    bool cut = false;   // demo switch: the endpoint refuses packets as if the link dropped

private:
    void makeReal(IntWorld& world, int adc);
    static void stockIfEmpty(IntWorld& world, int adc);

    long long lastPacketMs = -1;
    std::set<int> real;
};
