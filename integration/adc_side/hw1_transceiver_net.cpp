//HW-1 transceiver, networked (build-time replacement)

/*
Replaces Subgroup_A/src/HW1_Transceiver.cpp in the integrated ADC build only
(integration/makefile). Subgroup A's own build and file are unchanged.

Same interface (Subgroup_A/inc/HW1_Transceiver.hpp) and the same console lines
as the original stub. Behind them is a real link to the HMS:
  Receive_Message   "HMS_ACK" while the HMS answers, "" while it doesn't
  Send_Message      sends the text as part of a heartbeat; 1 if the HMS is up, else 0
  Check_Connection  a real ping: 1 if the HMS answers, 0 if not. On 0, Subgroup
                    A's BH0 puts the ADC in Offline mode (its own behaviour) and a
                    CONNECTION_LOSS alarm is queued for the HMS
It also starts the integration before main() (hw1_net::start, called by
hw1_autostart.cpp):
  - the demo cabinet selection: which registered ADC this screen is, with that
    cabinet's stock loaded from the HMS (cabinet.cpp); "[4] Swap ADC" on the
    login menu changes it later
  - the console observer, so the ADC's dispenses and alarms reach the HMS
    (adc_output_table.cpp), each stamped with the local time its line was printed
  - the console input filter that takes the "4" for Swap ADC (console_input.cpp)

Settings (environment variables, all optional):
  HMS_HOST (127.0.0.1), HMS_PORT (9040), HMS_KEY (run/data/key.bin),
  ADC_HEARTBEAT_SECONDS (20), ADC_ID (skips the selection screen at start)
*/

#include "HW1_Transceiver.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>

#include "adc_link.hpp"
#include "adc_output_table.hpp"
#include "cabinet.hpp"
#include "console_input.hpp"
#include "console_observer.hpp"
#include "hw1_net.hpp"

namespace {

const int DEFAULT_ADC = 3;   // registered by hms_live for the cabinet program (ADC_HMS_INTERFACE.md, D4)

std::string setting(const char* name, const char* fallback) {
    const char* value = std::getenv(name);
    return value && *value ? value : fallback;
}

// This computer's local time, "YYYY-MM-DD HH:MM:SS": when the ADC printed the line.
std::string localNow() {
    std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm local = *std::localtime(&t);
    char text[20];
    std::strftime(text, sizeof text, "%Y-%m-%d %H:%M:%S", &local);
    return text;
}

std::string trimmed(const std::string& s) {
    std::size_t a = s.find_first_not_of(" \t"), b = s.find_last_not_of(" \t");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

// Set up before main() by hw1_autostart.cpp (Subgroup A's main is unchanged),
// stopped after it returns. Before main() it only puts in the console hooks:
// they use nothing of Subgroup A's, whose static data (e.g. SW1's stock) may not
// exist yet. The cabinet selection and the link start just before Subgroup A's
// first output, when main() is running. The link gets ~3 s at exit to send
// what is still queued.
struct Integration {
    std::string host;
    int port = 0;
    int heartbeat = 20;
    std::optional<std::vector<uint8_t>> key;
    std::unique_ptr<HmsClient> hms;   // for the cabinet requests; the link has its own
    std::unique_ptr<AdcLink> link;
    std::unique_ptr<ConsoleObserver> observer;
    std::unique_ptr<ConsoleInput> input;
    std::streambuf* console = nullptr;
    std::streambuf* keyboard = nullptr;
    std::unique_ptr<std::ostream> screen;   // the integration's own lines, straight to the console
    std::atomic<int> adc{DEFAULT_ADC};      // BH3's drawer-safety thread prints too
    std::string location;
    std::atomic<bool> atLoginMenu{false};   // the next keyboard line answers Subgroup A's login menu
    bool started = false;

    Integration() {
        std::string keyPath = setting("HMS_KEY", "run/data/key.bin");
        host = setting("HMS_HOST", "127.0.0.1");
        port = std::atoi(setting("HMS_PORT", "9040").c_str());
        heartbeat = std::atoi(setting("ADC_HEARTBEAT_SECONDS", "20").c_str());
        if (heartbeat <= 0) heartbeat = 20;
        key = readKeyFile(keyPath);
        if (!key) {
            std::cerr << "[HMS-LINK] No HMS key at " << keyPath << " (start hms_live first): "
                      << "this ADC runs on its own and sends nothing to the HMS\n";
            return;
        }
        hms = std::make_unique<HmsClient>(host, port, *key);
        console = std::cout.rdbuf();
        screen = std::make_unique<std::ostream>(console);
        observer = std::make_unique<ConsoleObserver>(
            console, [this](const std::string& line) { onLine(line); }, [this] { start(); });
        std::cout.rdbuf(observer.get());
        keyboard = std::cin.rdbuf();
        input = std::make_unique<ConsoleInput>(keyboard, [this](const std::string& line) { return onKeyboard(line); });
        std::cin.rdbuf(input.get());
    }

    ~Integration() {
        if (input) std::cin.rdbuf(keyboard);
        if (observer) std::cout.rdbuf(console);
        link.reset();   // sends what it can, then stops
    }

    // The cabinet selection, then the link. Once; main() is running by now.
    void start() {
        if (started || !key) return;
        started = true;
        selectAtStart();
        link = std::make_unique<AdcLink>(adc, std::make_unique<HmsClient>(host, port, *key), heartbeat);
    }

    // ---- What the ADC prints (Subgroup A's code, through the observer) ----
    void onLine(const std::string& line) {
        if (!link) return;
        if (std::optional<std::string> message = adc_output::translate(adc, line)) {
            link->enqueue(adc_output::stamp(*message, localNow()));
        }
        // The demo selection, shown on Subgroup A's own screens.
        std::string extra;
        if (line == cabinet::HEADER_LINE) extra = "   This screen: ADC-" + std::to_string(adc) + ", " + location + " (demo)\n";
        if (line == cabinet::LOGIN_MENU_LAST) {
            extra = std::string(cabinet::SWAP_OPTION) + "\n";
            atLoginMenu = true;
        }
        if (!extra.empty()) console->sputn(extra.data(), static_cast<std::streamsize>(extra.size()));
    }

    // ---- What is typed (before Subgroup A's code reads it) ----
    bool onKeyboard(const std::string& line) {
        bool answersLoginMenu = atLoginMenu.exchange(false);
        if (!link || !answersLoginMenu || trimmed(line) != cabinet::SWAP_KEY) return false;
        swap();
        *screen << "\n [1] Secure User Login  [2] Run Automated Test Suite  [3] Shutdown System  [4] Swap ADC (demo only)\n"
                << " Select an option: " << std::flush;
        atLoginMenu = true;   // the next line answers the login menu again
        return true;
    }

    // ---- Selection ----
    // Loads the cabinet's stock from the HMS. False (with the reason shown) if it can't.
    bool claim(int id, int previous) {
        std::optional<std::string> answer = hms->ask("/adc/claim", std::to_string(id) + "|Claim|" + std::to_string(previous));
        std::string error = "the HMS did not answer";
        std::optional<cabinet::Claimed> c = answer ? cabinet::parseClaim(*answer, &error) : std::nullopt;
        if (!c) {
            *screen << " Could not open ADC-" << id << ": " << error << ".\n" << std::flush;
            return false;
        }
        cabinet::loadStock(c->stock);
        adc = id;
        location = c->location;
        *screen << " This screen is now ADC-" << id << " (" << location << "). Stock loaded from the HMS: "
                << cabinet::describe(c->stock) << ".\n" << std::flush;
        return true;
    }

    void selectAtStart() {
        std::string fixed = setting("ADC_ID", "");
        if (!fixed.empty()) {   // scripts and tests
            int id = std::atoi(fixed.c_str());
            adc = id > 0 ? id : DEFAULT_ADC;
            if (!claim(adc, 0)) *screen << " Running as ADC-" << adc << " with Subgroup A's own stock.\n" << std::flush;
            if (location.empty()) location = "(not from the HMS)";
            return;
        }
        while (true) {
            std::optional<std::string> answer = hms->ask("/adc/cabinets", "List");
            std::vector<cabinet::Cabinet> list = answer ? cabinet::parseList(*answer) : std::vector<cabinet::Cabinet>{};
            if (list.empty()) {
                *screen << "\n The HMS is not reachable (start hms_live first), so the cabinets can't be listed.\n"
                        << " Press Enter to try again, or type an ADC number to start without the HMS: " << std::flush;
                std::optional<std::string> typed = input->readLine();
                if (!typed) return;
                if (std::atoi(typed->c_str()) > 0) {
                    adc = std::atoi(typed->c_str());
                    location = "(not from the HMS)";
                    *screen << " Running as ADC-" << adc << " with Subgroup A's own stock; it will not match the HMS.\n"
                            << std::flush;
                    return;
                }
                continue;
            }
            int id = cabinet::choose(list, 0, *screen, [this] { return input->readLine(); });
            if (id != 0 && claim(id, 0)) return;
            if (id == 0) *screen << " Please type one of the ADC numbers listed.\n" << std::flush;
        }
    }

    void swap() {
        if (!link->up() || link->waiting() > 0) {
            *screen << "\n Can't swap ADC now: " << (link->up() ? std::to_string(link->waiting()) +
                       " message(s) are still waiting to be sent to the HMS" : std::string("the HMS is not reachable"))
                    << ". Try again in a moment.\n";
            return;
        }
        std::optional<std::string> answer = hms->ask("/adc/cabinets", "List");
        std::vector<cabinet::Cabinet> list = answer ? cabinet::parseList(*answer) : std::vector<cabinet::Cabinet>{};
        if (list.empty()) {
            *screen << "\n Can't swap ADC now: the HMS did not send the cabinet list.\n";
            return;
        }
        int id = cabinet::choose(list, adc, *screen, [this] { return input->readLine(); });
        if (id == 0) { *screen << " No change: still ADC-" << adc << ".\n"; return; }
        if (id == adc) { *screen << " Already ADC-" << adc << ".\n"; return; }
        int previous = adc;
        if (claim(id, previous)) link->setAdcId(id);
        else adc = previous;
    }
};

Integration& integration() {
    static Integration instance;
    return instance;
}

}  // namespace

namespace hw1_net {
    void prepare() { integration(); }
    void start() { integration().start(); }
    AdcLink* link() { return integration().link.get(); }
    int adcId() { return integration().adc; }
}

namespace HW1 {
    std::string Receive_Message() {
        std::cout << "[HW-1] Reading HMS receiver buffer...\n";
        AdcLink* link = integration().link.get();
        return link && link->up() ? "HMS_ACK" : "";
    }

    int Send_Message(std::string mess_content) {
        std::cout << "[HW-1] Transmitting to HMS: " << mess_content << "\n";
        AdcLink* link = integration().link.get();
        if (!link) return 0;
        link->enqueue(std::to_string(integration().adc) + "|Heartbeat|" + adc_output::field(mess_content));
        return link->up() ? 1 : 0;
    }

    int Check_Connection() {
        std::cout << "[HW-1] Polling HMS connection...\n";
        AdcLink* link = integration().link.get();
        if (link && link->pingNow()) return 1;
        if (link) {
            link->enqueue(adc_output::stamp(std::to_string(integration().adc)
                          + "|Alarm|CONNECTION_LOSS|ADC ping to the HMS failed; ADC in offline mode (local dispensing only)",
                          localNow()));
        }
        return 0;
    }
}
