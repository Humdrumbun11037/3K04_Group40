// adc_stub: a stand-in ADC for testing hms_live, and the demo backup if the
// real cabinet won't start. Sends each message on the command line to the HMS
// exactly as the real ADC link does (encrypted with the shared key). Run from integration/.
//
//   adc_stub "3|Withdrawal|Tylenol|60|nurse_jane" "3|Alarm|UNAUTHORIZED_ACCESS|lockout"
//   adc_stub --heartbeats 20        send 3|Heartbeat every 20 s until stopped
//   adc_stub --wrong-key "3|Heartbeat"   a packet the HMS must refuse (Malformed)
//   adc_stub --ping
//   adc_stub --claim 3 ...          take ADC-3 over from the simulator first (as the cabinet program does)
// Options: --key <file> (default run/data/key.bin), --port <n> (default 9040).

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "adc_side/hms_client.hpp"

static const char* const RESULT[] = {"Acked", "Refused", "Unreachable"};

int main(int argc, char** argv) {
    std::string keyPath = "run/data/key.bin";
    int port = 9040;
    bool wrongKey = false, ping = false;
    int heartbeatSeconds = 0;
    int claim = 0;
    std::vector<std::string> messages;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--key" && i + 1 < argc) keyPath = argv[++i];
        else if (a == "--port" && i + 1 < argc) port = std::atoi(argv[++i]);
        else if (a == "--heartbeats" && i + 1 < argc) heartbeatSeconds = std::atoi(argv[++i]);
        else if (a == "--claim" && i + 1 < argc) claim = std::atoi(argv[++i]);
        else if (a == "--wrong-key") wrongKey = true;
        else if (a == "--ping") ping = true;
        else messages.push_back(a);
    }

    std::optional<std::vector<uint8_t>> key = readKeyFile(keyPath);
    if (!key) {
        std::cerr << "No 32-byte key at " << keyPath << ". Start hms_live first (it creates the key).\n";
        return 2;
    }
    if (wrongKey) (*key)[0] ^= 0xFF;
    HmsClient hms("127.0.0.1", port, *key);

    if (ping) {
        bool ok = hms.ping();
        std::cout << "ping " << hms.address() << ": " << (ok ? "HMS_ACK" : "no answer") << "\n";
        return ok ? 0 : 1;
    }
    int failures = 0;
    if (claim > 0) {
        std::optional<std::string> answer = hms.ask("/adc/claim", std::to_string(claim) + "|Claim|0");
        std::cout << "claim ADC-" << claim << " -> " << (answer ? *answer : "no answer") << "\n";
        if (!answer || answer->rfind("OK|", 0) != 0) failures++;
    }
    for (const std::string& m : messages) {
        std::string reply;
        HmsClient::Result r = hms.send(m, &reply);
        std::cout << m << " -> " << RESULT[static_cast<int>(r)] << (reply.empty() ? "" : " (" + reply + ")") << "\n";
        if (r != HmsClient::Result::Acked) failures++;
    }
    while (heartbeatSeconds > 0) {
        HmsClient::Result r = hms.send("3|Heartbeat");
        std::cout << "3|Heartbeat -> " << RESULT[static_cast<int>(r)] << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(heartbeatSeconds));
    }
    return failures == 0 ? 0 : 1;
}
