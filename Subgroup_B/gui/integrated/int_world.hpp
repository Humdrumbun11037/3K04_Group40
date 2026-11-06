//integrated world

/*
Hides: how the integrated program wires Andrew's, Inodh's and Matthew's real
       code together, and how the simulated parts behave (the clock and the
       ADCs, which send encrypted packets: a heartbeat a minute unless silenced)

Interface
takes: simulation commands (advance the clock, silence or resume an ADC, send a
       raw ADC message, make storage or report sending fail) and user actions
sends: the modules themselves for the API to call, a feed of what happened,
       each user's dashboard messages, and the Components list (REAL or SIMULATED)
uses: every behaviour module; Inodh's registry, users, authorization, encryption,
      key storage, patient info stripping, event scanner, message router and alarm
      notification; Matthew's encrypted long-term storage (history); the adapters
      in gui/integrated/adapters/; tests/fakes/ for what is simulated

Storage: every world starts a fresh audit trail in <dataDir>/active.log and
archive.log (encrypted, one record per line); the key in <dataDir>/key.bin is
kept between runs. gui/data/ is git-ignored.

Message path, per tick: each ADC's queue gives at most one packet to the router
(routePacket), never pollLink, so a busy or broken link can't hold up the loop.
Separate from the simulation-mode GUI (gui/hms_world.*), which is unchanged.
*/
#pragma once

#include "gui/integrated/sw_names.hpp"

#include <cstdint>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "behaviour_hiding/adc_heartbeats.hpp"
#include "behaviour_hiding/alarm_resolution.hpp"
#include "behaviour_hiding/hospital_stock.hpp"
#include "behaviour_hiding/inventory_record.hpp"
#include "behaviour_hiding/maintenance.hpp"
#include "behaviour_hiding/report_formatting.hpp"
#include "behaviour_hiding/report_types.hpp"
#include "gui/integrated/adapters/alarm_adapters.hpp"
#include "gui/integrated/adapters/basic_adapters.hpp"
#include "gui/integrated/adapters/file_history.hpp"
#include "gui/integrated/adapters/message_adapters.hpp"
#include "gui/integrated/feed.hpp"
#include "tests/fakes/fake_clock.hpp"
#include "tests/fakes/fake_heartbeat_counter.hpp"
#include "tests/fakes/fake_history.hpp"
#include "tests/fakes/fake_permissions.hpp"
#include "tests/fakes/fake_report_sender.hpp"

// One part of the system and whether the demo runs the real code for it.
struct Component {
    std::string name;
    std::string owner;
    bool real;
    std::string note;
};

// A maintenance window scheduled through the GUI (Maintenance has no query
// that lists its windows; it still decides every verdict).
struct IntWindow {
    hms::behaviour::AdcId adc;
    long long start;
    long long end;
    bool cancelled;
};

// What happened to one raw ADC message on its way through the message path.
struct MessageTrace {
    std::string raw;            // as the simulated ADC wrote it, patient fields included
    std::string encryptedHex;   // the start of the packet that crossed the link
    std::size_t packetBytes = 0;
    std::string decrypted;      // what Inodh's encryption recovered
    std::string stripped;       // what Inodh's patient info stripping let through
    std::vector<std::string> removed;   // fields the stripping took out
    std::string outcome;        // the router's verdict, or the error
};

class IntWorld {
public:
    static constexpr long long START_TIME = 1793598000;   // 2026-11-02 05:40:00 local
    static constexpr long long TICK_SECONDS = 10;
    static constexpr long long HEARTBEAT_SECONDS = 60;
    static constexpr long long MAX_ADVANCE = 2 * 24 * 60 * 60;
    static constexpr std::size_t QUEUE_LIMIT = 20;       // packets waiting per simulated ADC

    struct DemoAccount { const char* id; const char* password; UserRole role; };
    static constexpr DemoAccount DEMO_ACCOUNTS[] = {
        {"tara", "tara-demo", UserRole::Technician},
        {"priya", "priya-demo", UserRole::Pharmacist},
        {"alex", "alex-demo", UserRole::Administrator},
    };

    // ADC-1 (Ward 3 West) and ADC-2 (ICU) registered with a day of heartbeats,
    // the same stock and reserves as the simulation mode, and the demo accounts.
    explicit IntWorld(const std::string& dataDir);
    IntWorld(const IntWorld&) = delete;
    IntWorld& operator=(const IntWorld&) = delete;

    // Registry changes go through Inodh's ADClist; monitoring follows them.
    ADClist::Result registerAdc(int id, const std::string& location, const std::string& byUser);
    ADClist::Result relocateAdc(int id, const std::string& location, const std::string& byUser);
    ADClist::Result removeAdc(int id, const std::string& byUser);

    Authorization::LoginResult login(const std::string& id, const std::string& password);
    void logout();
    // True if someone is logged in and their session is still live; renews it
    // and refreshes `user` from the session.
    bool touchSession();

    // A simulated ADC writes a raw message; it is encrypted, queued, and the
    // ADC's queue gets one routing turn. Returns what happened on the way.
    MessageTrace sendMessage(const std::string& raw);
    // One heartbeat packet from a simulated ADC, routed straight away.
    void sendHeartbeat(const hms::behaviour::AdcId& adc);
    // An ADC upload of a withdrawal or deposit, as a message through the router.
    MessageTrace upload(const hms::behaviour::AdcId& adc, const std::string& keyword,
                        const std::string& medication, int amount);

    void advance(long long seconds);
    void periodicChecks();
    DailyVerdict runDaily();
    void setHistoryFailing(bool failing);
    void setSenderFailing(bool failing);
    void note(const std::string& kind, const std::string& text);
    std::string timeText(long long seconds) const { return format.formatTime(seconds); }
    std::vector<Component> components() const;
    std::size_t queued() const;

    // Simulated clock
    FakeClock clock{START_TIME};
    TextClock textClock{clock};

    // Storage: Matthew's encrypted audit trail, with Inodh's key storage and
    // encryption (the same EncryptionManager also opens the ADCs' packets)
    std::string dataDir;
    bool freshData;
    std::vector<uint8_t> key;
    EncryptionManager encryption;
    FileAuditTrail trail;
    FileHistory history{trail};
    FakeHeartbeatCounter heartbeatLog;
    FakeReportSender sender;
    ReportFormatting format;
    FakePermissions permissions;   // the agreed table, for Andrew's modules
    Feed feed;
    std::map<hms::behaviour::AdcId, long long> lastHeartbeat;

    // Inodh's registry, users and login
    EventToHistory recorder{history, clock};
    ADClist adcList{&recorder};
    RegistryAdapter registry{adcList};
    UserSettings settings;   // Inodh's table: used only by his own code
    UserRegistry users;
    Authorization auth{users, settings, recorder, textClock};

    // Alarms: Inodh's notification delivers, Andrew's resolution holds the state
    AlarmResolution alarms{clock, history, permissions};
    Inbox inbox{clock};
    AlarmLinks links{alarms, clock, feed};
    SeverityRules severityRules;
    ResolverLink resolver{links};
    ::AlarmNotifier swNotifier{severityRules, resolver, users, inbox, recorder, textClock};
    NotifierBridge notifier{swNotifier, links};

    // Andrew's modules
    Maintenance maintenance{clock, registry, history, permissions};
    AdcHeartbeats heartbeats{clock, maintenance, notifier, history};
    InventoryRecord inventory{clock, history, notifier, permissions};
    HospitalStock hospital{clock, inventory, history, notifier, permissions};
    ReportTypes reports{clock, history, history, heartbeatLog, registry, sender, permissions};

    // Inodh's message path into Andrew's modules
    PatientInfoStripper stripper;
    EventScanner scanner{textClock, recorder};
    HeartbeatLink heartbeatLink{heartbeats, heartbeatLog, lastHeartbeat, clock};
    InventoryLink inventoryLink{inventory, clock, feed};
    MessageRouter router{encryption, stripper, adcList, scanner, heartbeatLink, swNotifier, inventoryLink, textClock};

    std::string sessionUser;   // who is logged in ("" if nobody)
    hms::behaviour::User user{"", hms::behaviour::Role::Technician};   // the session user, for Andrew's modules
    std::set<hms::behaviour::AdcId> silenced;
    std::vector<IntWindow> windows;
    MessageTrace lastTrace;
    bool historyFailing = false;
    bool senderFailing = false;
    DailyVerdict lastDaily = DailyVerdict::TooEarly;

private:
    static bool startFresh(const std::string& dataDir);
    bool enqueue(int sender, const std::string& plaintext);
    std::string routeOne(int sender);
    void routeAll();
    void noteStatusChanges();
    std::map<int, std::deque<std::vector<uint8_t>>> outbox;   // by sender ADC id (0 = unreadable id)
    std::map<hms::behaviour::AdcId, AdcStatus> shownStatus;
};
