//HMS client

#include "hms_client.hpp"

#include <fstream>
#include <iterator>

#include "httplib.h"
#include "message_processing/encryption.hpp"

static const time_t CONNECT_SECONDS = 1;
static const time_t READ_SECONDS = 2;

HmsClient::HmsClient(std::string host, int port, std::vector<uint8_t> key)
    : host(std::move(host)), port(port),
      encryption(std::make_unique<EncryptionManager>(EncryptionManager::defaultStrategy(), std::move(key))) {}

HmsClient::~HmsClient() = default;

static httplib::Client client(const std::string& host, int port) {
    httplib::Client c(host, port);
    c.set_connection_timeout(CONNECT_SECONDS, 0);
    c.set_read_timeout(READ_SECONDS, 0);
    c.set_write_timeout(READ_SECONDS, 0);
    return c;
}

HmsClient::Result HmsClient::send(const std::string& message, std::string* reply) {
    return sendRaw(encryption->encryptForADC(std::vector<uint8_t>(message.begin(), message.end())), reply);
}

HmsClient::Result HmsClient::sendRaw(const std::vector<uint8_t>& packet, std::string* reply) {
    httplib::Client c = client(host, port);
    httplib::Result r = c.Post("/adc/packet", std::string(packet.begin(), packet.end()), "application/octet-stream");
    if (!r) return Result::Unreachable;
    if (reply) *reply = r->body;
    return r->status == 200 ? Result::Acked : Result::Refused;
}

bool HmsClient::ping() {
    httplib::Client c = client(host, port);
    httplib::Result r = c.Get("/adc/ping");
    return r && r->status == 200 && r->body.rfind("HMS_ACK", 0) == 0;
}

std::optional<std::string> HmsClient::ask(const std::string& path, const std::string& message) {
    std::vector<uint8_t> packet = encryption->encryptForADC(std::vector<uint8_t>(message.begin(), message.end()));
    httplib::Client c = client(host, port);
    httplib::Result r = c.Post(path, std::string(packet.begin(), packet.end()), "application/octet-stream");
    if (!r || r->status != 200) return std::nullopt;
    try {
        std::vector<uint8_t> plain = encryption->decryptADCMessage(std::vector<uint8_t>(r->body.begin(), r->body.end()));
        return std::string(plain.begin(), plain.end());
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::optional<std::vector<uint8_t>> readKeyFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::nullopt;
    std::vector<uint8_t> key((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (key.size() != EncryptionManager::kKeySize) return std::nullopt;
    return key;
}
