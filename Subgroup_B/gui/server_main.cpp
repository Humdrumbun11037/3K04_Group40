// HMS GUI server: serves the page in gui/web/ and the API (gui/api.cpp) on
// http://127.0.0.1:8080 only. This file is the only one that knows about HTTP;
// it passes each /api/ request to HmsApi and sends back its JSON answer.
// It makes no network calls of its own, and the page loads nothing from outside.

#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>

#include "third_party/httplib.h"

#include "gui/api.hpp"

static const char* const HOST = "127.0.0.1";
static const int PORT = 8080;

// The page lives in gui/web/; works whether started from Subgroup_B/gui or Subgroup_B.
static std::string findWebFolder() {
    for (const char* candidate : {"web", "gui/web"}) {
        if (std::filesystem::exists(std::string(candidate) + "/index.html")) return candidate;
    }
    return "";
}

// Only requests addressed to this machine by name are answered, so a web page
// from elsewhere can't reach the HMS through the browser (DNS rebinding).
static bool isLocalHost(const std::string& host) {
    const std::string port = ":" + std::to_string(PORT);
    return host == "127.0.0.1" + port || host == "localhost" + port;
}

int main() {
    std::string web = findWebFolder();
    if (web.empty()) {
        std::cerr << "Cannot find web/index.html. Start the GUI from Subgroup_B/gui (mingw32-make run).\n";
        return 1;
    }

    HmsApi api;
    std::mutex worldLock;   // the server answers on several threads; the HMS world is used by one at a time
    httplib::Server server;

    server.set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        if (isLocalHost(req.get_header_value("Host"))) return httplib::Server::HandlerResponse::Unhandled;
        res.status = 403;
        res.set_content("{\"ok\":false,\"message\":\"only local requests are answered\"}", "application/json");
        return httplib::Server::HandlerResponse::Handled;
    });

    auto toApi = [&](const httplib::Request& req, httplib::Response& res) {
        ApiRequest request{req.method, req.path, {}};
        for (const auto& [key, value] : req.params) request.params[key] = value;
        ApiResponse answer;
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

    if (!server.set_mount_point("/", web)) {
        std::cerr << "Cannot serve the folder " << web << "\n";
        return 1;
    }

    std::cout << "HMS GUI: open http://" << HOST << ":" << PORT << " in a browser. Ctrl+C stops it.\n";
    if (!server.listen(HOST, PORT)) {
        std::cerr << "Could not listen on " << HOST << ":" << PORT << ". Is another copy already running?\n";
        return 1;
    }
    return 0;
}
