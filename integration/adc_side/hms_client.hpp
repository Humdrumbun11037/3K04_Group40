//HMS client

/*
Hides: how an ADC message reaches the HMS: HTTP over TCP (cpp-httplib),
       the endpoint paths, the timeouts, and the encryption (Inodh's
       EncryptionManager with the pre-shared key). See ADC_HMS_INTERFACE.md.

Interface
takes: the HMS host and port, the key, plain-text ADC messages
sends: encrypted packets to POST /adc/packet; pings to GET /adc/ping
uses: sw_decisions/message_processing/encryption (unchanged), cpp-httplib
*/
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class EncryptionManager;

class HmsClient {
public:
    enum class Result { Acked, Refused, Unreachable };

    HmsClient(std::string host, int port, std::vector<uint8_t> key);
    ~HmsClient();

    // Encrypts and sends one message. `reply` gets the HMS's answer text.
    Result send(const std::string& message, std::string* reply = nullptr);
    // Sends bytes as they are (adc_stub uses this to send a bad packet).
    Result sendRaw(const std::vector<uint8_t>& packet, std::string* reply = nullptr);
    // True if the HMS answers HMS_ACK to a ping.
    bool ping();

    // Demo cabinet selection (ADC_HMS_INTERFACE.md section 7): sends an
    // encrypted request to `path` and returns the HMS's decrypted answer, or
    // nothing if the HMS is unreachable, refuses, or the answer won't decrypt.
    std::optional<std::string> ask(const std::string& path, const std::string& message);

    std::string address() const { return host + ":" + std::to_string(port); }

private:
    std::string host;
    int port;
    std::unique_ptr<EncryptionManager> encryption;
};

// The 32-byte key file, or nothing if it is missing or the wrong size.
// Never creates one: the HMS does, on its first start.
std::optional<std::vector<uint8_t>> readKeyFile(const std::string& path);
