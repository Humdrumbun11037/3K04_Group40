// hms_live: Subgroup B's integrated HMS (Subgroup_B/gui/integrated, unchanged)
// joined to the real Subgroup A cabinet. Start it from integration/.
//   http://127.0.0.1:8081   the HMS dashboard (Subgroup B's page, plus a 2 s auto-refresh)
//   127.0.0.1:9040          the ADC endpoint: POST /adc/packet, GET /adc/ping,
//                           and (demo cabinet selection) POST /adc/cabinets, /adc/claim
//                           (0.0.0.0:9040 with --listen-all, for an ADC on another machine)
// The simulated clock runs in real time. Commands typed in this window:
//   cut      drop the ADC link (the endpoint answers 503, as if the network failed)
//   restore  bring it back
//   status   show the link and the ADCs the cabinet program drives
//   quit     stop
// The only HTTP code on the HMS side; it makes no network calls of its own.

#include "gui/integrated/sw_names.hpp"

#include <atomic>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#include "third_party/httplib.h"

#include "gui/integrated/int_api.hpp"
#include "hms_side/live_adc.hpp"

static const char* const HOST = "127.0.0.1";
static const int DASHBOARD_PORT = 8081;
static const int ADC_PORT = 9040;
static const char* const DATA_DIR = "run/data";
static const char* const WEB = "../Subgroup_B/gui/integrated/web";
static const char* const COMMON = "../Subgroup_B/gui/web";
static const char* const LIVE_JS = "hms_side/web/live.js";

static long long nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// This computer's local time, "YYYY-MM-DD HH:MM:SS", to compare with the ADC's ADC_TIME stamps.
static std::string localNow() {
    std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm local = *std::localtime(&t);
    char text[20];
    std::strftime(text, sizeof text, "%Y-%m-%d %H:%M:%S", &local);
    return text;
}

static std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

// Only requests addressed to this machine by name are answered (as Subgroup B's server).
static bool isLocalHost(const std::string& host, int port) {
    const std::string p = ":" + std::to_string(port);
    return host == "127.0.0.1" + p || host == "localhost" + p;
}

int main(int argc, char** argv) {
    // --listen-all: the ADC endpoint also answers other machines (two-laptop
    // demo; copy run/data/key.bin to the ADC machine). The dashboard stays local.
    bool listenAll = argc > 1 && std::string(argv[1]) == "--listen-all";
    const char* adcHost = listenAll ? "0.0.0.0" : HOST;
    if (!std::filesystem::exists(std::string(WEB) + "/index.html") || !std::filesystem::exists(LIVE_JS)) {
        std::cerr << "Cannot find the page files. Start hms_live from the integration/ folder.\n";
        return 1;
    }
    std::filesystem::create_directories(DATA_DIR);

    std::unique_ptr<IntegratedApi> apiHolder;
    try {
        apiHolder = std::make_unique<IntegratedApi>(DATA_DIR);
    } catch (const std::exception& e) {
        std::cerr << "Cannot open the HMS storage in " << DATA_DIR << ": " << e.what() << "\n";
        return 1;
    }
    IntegratedApi& api = *apiHolder;
    LiveAdc link;
    std::mutex worldLock;   // dashboard, ADC endpoint, clock and console all use the one world
    LiveAdc::setup(api.world());

    // ---- Dashboard ----
    httplib::Server dashboard;
    dashboard.set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        if (isLocalHost(req.get_header_value("Host"), DASHBOARD_PORT)) return httplib::Server::HandlerResponse::Unhandled;
        res.status = 403;
        res.set_content("{\"ok\":false,\"message\":\"only local requests are answered\"}", "application/json");
        return httplib::Server::HandlerResponse::Handled;
    });
    auto page = [](const httplib::Request&, httplib::Response& res) {
        // Subgroup B's page with the standard accounts in its login hint and the
        // auto-refresh script added before </body>.
        std::string html = LiveAdc::correctPage(readFile(std::string(WEB) + "/index.html"));
        std::size_t end = html.rfind("</body>");
        html.insert(end == std::string::npos ? html.size() : end, "<script src=\"/integration/live.js\"></script>\n");
        res.set_header("Cache-Control", "no-store");
        res.set_content(html, "text/html");
    };
    dashboard.Get("/", page);
    dashboard.Get("/index.html", page);
    dashboard.Get("/int.js", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(readFile(std::string(WEB) + "/int.js"), "text/javascript");
    });
    dashboard.Get("/int.css", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(readFile(std::string(WEB) + "/int.css"), "text/css");
    });
    dashboard.Get("/integration/live.js", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(readFile(LIVE_JS), "text/javascript");
    });
    auto toApi = [&](const httplib::Request& req, httplib::Response& res) {
        IntRequest request{req.method, req.path, {}};
        for (const auto& [key, value] : req.params) request.params[key] = value;
        IntResponse answer;
        {
            std::lock_guard<std::mutex> guard(worldLock);
            std::optional<IntResponse> refused = link.refuse(request, nowMs());
            answer = refused ? *refused : api.handle(request);
            if (!refused) link.after(api, request, answer);
        }
        res.status = answer.status;
        res.set_header("Cache-Control", "no-store");
        res.set_content(answer.body, "application/json");
    };
    dashboard.Get(R"(/api/.*)", toApi);
    dashboard.Post(R"(/api/.*)", toApi);
    if (!dashboard.set_mount_point("/common", COMMON)) {
        std::cerr << "Cannot serve " << COMMON << "\n";
        return 1;
    }

    // ---- ADC endpoint ----
    httplib::Server adcEndpoint;
    adcEndpoint.set_payload_max_length(4096);
    adcEndpoint.Post("/adc/packet", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> guard(worldLock);
        if (link.cut) { res.status = 503; res.set_content("LINK CUT", "text/plain"); return; }
        std::string outcome = link.receive(api.world(), std::vector<uint8_t>(req.body.begin(), req.body.end()), nowMs(),
                                           localNow());
        res.set_content("HMS_ACK " + outcome, "text/plain");
    });
    // Demo cabinet selection: encrypted request, encrypted answer (ADC_HMS_INTERFACE.md section 7).
    auto cabinetRequest = [&](bool isClaim) {
        return [&, isClaim](const httplib::Request& req, httplib::Response& res) {
            std::lock_guard<std::mutex> guard(worldLock);
            if (link.cut) { res.status = 503; res.set_content("LINK CUT", "text/plain"); return; }
            IntWorld& w = api.world();
            std::string request;
            try {
                std::vector<uint8_t> plain = w.encryption.decryptADCMessage(std::vector<uint8_t>(req.body.begin(), req.body.end()));
                request.assign(plain.begin(), plain.end());
            } catch (const std::exception&) {
                res.status = 400;
                res.set_content("cannot decrypt", "text/plain");
                return;
            }
            std::string answer = isClaim ? link.claim(w, request) : link.cabinets(w);
            std::vector<uint8_t> sealed = w.encryption.encryptForADC(std::vector<uint8_t>(answer.begin(), answer.end()));
            res.set_content(std::string(sealed.begin(), sealed.end()), "application/octet-stream");
        };
    };
    adcEndpoint.Post("/adc/cabinets", cabinetRequest(false));
    adcEndpoint.Post("/adc/claim", cabinetRequest(true));
    adcEndpoint.Get("/adc/ping", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> guard(worldLock);
        if (link.cut) { res.status = 503; res.set_content("LINK CUT", "text/plain"); return; }
        res.set_content("HMS_ACK", "text/plain");
    });

    if (!dashboard.bind_to_port(HOST, DASHBOARD_PORT)) {
        std::cerr << "Could not listen on " << HOST << ":" << DASHBOARD_PORT
                  << ". Is another HMS (hms_live or hms_integrated) already running?\n";
        return 1;
    }
    if (!adcEndpoint.bind_to_port(adcHost, ADC_PORT)) {
        std::cerr << "Could not listen on " << adcHost << ":" << ADC_PORT << ". Is another hms_live running?\n";
        return 1;
    }
    std::thread dashboardThread([&] { dashboard.listen_after_bind(); });
    std::thread adcThread([&] { adcEndpoint.listen_after_bind(); });

    // ---- Real-time clock: the simulated clock moves with the wall clock ----
    std::atomic<bool> running{true};
    std::thread ticker([&] {
        long long last = nowMs();
        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            long long seconds = (nowMs() - last) / 1000;
            if (seconds <= 0) continue;
            last += seconds * 1000;
            std::lock_guard<std::mutex> guard(worldLock);
            api.world().advance(seconds);
        }
    });

    std::cout << "hms_live: the integrated HMS, joined to Subgroup A's cabinet program (adc_hms_link)\n"
              << "  Dashboard:    http://" << HOST << ":" << DASHBOARD_PORT << "\n"
              << "  Accounts:     pharm_bob / hash456 (Pharmacist), tech_tom / hash789 (Technician), "
                 "admin_alice / hash000 (Administrator)\n"
              << "  ADC endpoint: " << adcHost << ":" << ADC_PORT << "   key: " << DATA_DIR << "/key.bin"
              << (listenAll ? "  (all interfaces: allow port 9040 in the firewall)" : "") << "\n"
              << "  Type cut, restore, status or quit.\n";

    // ---- Console commands (if there is no console, just keep serving) ----
    for (std::string command; running && std::getline(std::cin, command);) {
        std::lock_guard<std::mutex> guard(worldLock);
        IntWorld& w = api.world();
        if (command == "cut") {
            link.cut = true;
            w.note("scenario", "ADC link cut (simulated network failure): the cabinet program's packets are refused");
            std::cout << "ADC link CUT. Packets and pings from the ADC are refused.\n";
        } else if (command == "restore") {
            link.cut = false;
            w.note("scenario", "ADC link restored");
            std::cout << "ADC link restored.\n";
        } else if (command == "status") {
            std::cout << "Link " << (link.cut ? "CUT" : link.live(nowMs()) ? "live" : "quiet (nothing in the last 30 s)")
                      << "; HMS time " << w.timeText(w.clock.now()) << "\n";
            if (link.realAdcs().empty()) std::cout << "  No ADC is driven by a cabinet program yet.\n";
            for (int adc : link.realAdcs()) {
                std::cout << "  ADC-" << adc << " " << (w.heartbeats.status(adc) == AdcStatus::Online ? "Online" : "Offline") << ":";
                for (const StockItem& item : w.inventory.items()) {
                    if (item.adc == adc) std::cout << " " << item.medication << " " << item.count;
                }
                std::cout << "\n";
            }
        } else if (command == "quit") {
            running = false;
        } else if (!command.empty()) {
            std::cout << "Commands: cut, restore, status, quit\n";
        }
    }
    while (running) std::this_thread::sleep_for(std::chrono::hours(1));   // no console (stdin closed): serve until killed
    dashboard.stop();
    adcEndpoint.stop();
    ticker.join();
    dashboardThread.join();
    adcThread.join();
    return 0;
}
