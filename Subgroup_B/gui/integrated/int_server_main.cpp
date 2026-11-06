// Integrated HMS GUI server: serves gui/integrated/web/ and the integrated API
// (gui/integrated/int_api.cpp) on http://127.0.0.1:8081 only. Port 8081, so it
// can run next to the simulation-mode GUI (8080). This file is the only one
// that knows about HTTP. It makes no network calls of its own.

#include "gui/integrated/sw_names.hpp"

#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>

#include "third_party/httplib.h"

#include "gui/integrated/int_api.hpp"

static const char* const HOST = "127.0.0.1";
static const int PORT = 8081;

// Works whether started from Subgroup_B/gui (mingw32-make run-integrated) or Subgroup_B.
static std::string findFolder(const char* fromGui, const char* fromSubgroup, const char* mustHave) {
    for (const char* candidate : {fromGui, fromSubgroup}) {
        if (std::filesystem::exists(std::string(candidate) + "/" + mustHave)) return candidate;
    }
    return "";
}

// Only requests addressed to this machine by name are answered (DNS rebinding).
static bool isLocalHost(const std::string& host) {
    const std::string port = ":" + std::to_string(PORT);
    return host == "127.0.0.1" + port || host == "localhost" + port;
}

int main() {
    std::string web = findFolder("integrated/web", "gui/integrated/web", "index.html");
    std::string common = findFolder("web", "gui/web", "style.css");   // the simulation page's stylesheet, shared
    if (web.empty() || common.empty()) {
        std::cerr << "Cannot find the page files. Start it from Subgroup_B/gui (mingw32-make run-integrated).\n";
        return 1;
    }

    IntegratedApi api;
    std::mutex worldLock;   // requests arrive on several threads; the world is used by one at a time
    httplib::Server server;

    server.set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        if (isLocalHost(req.get_header_value("Host"))) return httplib::Server::HandlerResponse::Unhandled;
        res.status = 403;
        res.set_content("{\"ok\":false,\"message\":\"only local requests are answered\"}", "application/json");
        return httplib::Server::HandlerResponse::Handled;
    });

    auto toApi = [&](const httplib::Request& req, httplib::Response& res) {
        IntRequest request{req.method, req.path, {}};
        for (const auto& [key, value] : req.params) request.params[key] = value;
        IntResponse answer;
        {
            std::lock_guard<std::mutex> guard(worldLock);
            answer = api.handle(request);
        }
        res.status = answer.status;
        res.set_header("Cache-Control", "no-store");
        res.set_content(answer.body, "application/json");
    };
    server.Get(R"(/api/.*)", toApi);
    server.Post(R"(/api/.*)", toApi);

    if (!server.set_mount_point("/common", common) || !server.set_mount_point("/", web)) {
        std::cerr << "Cannot serve the page folders\n";
        return 1;
    }

    std::cout << "Integrated HMS GUI: open http://" << HOST << ":" << PORT << " in a browser. Ctrl+C stops it.\n"
              << "Demo accounts: tara / tara-demo (Technician), priya / priya-demo (Pharmacist), "
                 "alex / alex-demo (Administrator)\n";
    if (!server.listen(HOST, PORT)) {
        std::cerr << "Could not listen on " << HOST << ":" << PORT << ". Is another copy already running?\n";
        return 1;
    }
    return 0;
}
