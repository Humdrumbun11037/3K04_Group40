// A stand-in HMS for the ADC-side tests: the same endpoint as hms_live
// (POST /adc/packet, GET /adc/ping), decrypting with its own key, recording
// each message in order. It can be down (not started), up, or cut (503).
#pragma once

#include <atomic>
#include <cassert>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "httplib.h"
#include "message_processing/encryption.hpp"

struct FakeHms {
    explicit FakeHms(int port) : port(port) {}

    int port;
    std::vector<uint8_t> key = EncryptionManager::generateKey();
    EncryptionManager encryption{EncryptionManager::defaultStrategy(), key};
    httplib::Server server;
    std::thread thread;
    std::mutex lock;
    std::vector<std::string> received;
    std::atomic<bool> cut{false};

    void start() {
        server.Post("/adc/packet", [this](const httplib::Request& req, httplib::Response& res) {
            if (cut) { res.status = 503; return; }
            std::vector<uint8_t> plain = encryption.decryptADCMessage(std::vector<uint8_t>(req.body.begin(), req.body.end()));
            std::lock_guard<std::mutex> guard(lock);
            received.emplace_back(plain.begin(), plain.end());
            res.set_content("HMS_ACK Event", "text/plain");
        });
        server.Get("/adc/ping", [this](const httplib::Request&, httplib::Response& res) {
            if (cut) { res.status = 503; return; }
            res.set_content("HMS_ACK", "text/plain");
        });
        assert(server.bind_to_port("127.0.0.1", port));
        thread = std::thread([this] { server.listen_after_bind(); });
    }
    void stop() {
        server.stop();
        if (thread.joinable()) thread.join();
    }
    ~FakeHms() { stop(); }

    // Messages other than heartbeats, in arrival order.
    std::vector<std::string> events() {
        std::lock_guard<std::mutex> guard(lock);
        std::vector<std::string> out;
        for (const std::string& m : received) {
            if (m.find("|Heartbeat") == std::string::npos) out.push_back(m);
        }
        return out;
    }
    int heartbeats() {
        std::lock_guard<std::mutex> guard(lock);
        int n = 0;
        for (const std::string& m : received) n += m == "3|Heartbeat";
        return n;
    }
};

template <typename F>
bool within(int ms, F condition) {
    for (int waited = 0; waited < ms; waited += 50) {
        if (condition()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return condition();
}
