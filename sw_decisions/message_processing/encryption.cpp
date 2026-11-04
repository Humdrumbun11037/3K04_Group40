//encryption

/*
Hides: the encryption algorithm, the key, and the layout of an encrypted
       packet

Receives: encrypted packets from message_routing,
          report data from report_generation
Sends: decrypted messages back to message_routing,
       encrypted reports back to report_generation
*/

#include "encryption.hpp"

#include <array>
#include <random>
#include <stdexcept>
#include <vector>

#ifdef HMS_HAVE_OPENSSL
#include <openssl/evp.h>
#endif

namespace {

constexpr std::size_t kNonceSize = 12;
constexpr std::size_t kTagSize = 16;

std::vector<uint8_t> randomBytes(std::size_t n) {
    std::random_device rd;
    std::vector<uint8_t> out(n);
    for (std::size_t i = 0; i < n; i += 4) {
        uint32_t r = rd();
        for (std::size_t j = 0; j < 4 && i + j < n; ++j) {
            out[i + j] = static_cast<uint8_t>(r >> (8 * j));
        }
    }
    return out;
}

void requireKeySize(const std::vector<uint8_t>& key) {
    if (key.size() != EncryptionManager::kKeySize) {
        throw std::invalid_argument("Encryption key must be 32 bytes.");
    }
}

// Compares two tags in time that does not depend on where they differ.
bool constantTimeEqual(const uint8_t* a, const uint8_t* b, std::size_t n) {
    uint8_t diff = 0;
    for (std::size_t i = 0; i < n; ++i) diff |= static_cast<uint8_t>(a[i] ^ b[i]);
    return diff == 0;
}

// ------------------------------------------------------------ ChaCha20 (RFC 8439 2.3)

uint32_t load32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

void store32(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>(v);
    p[1] = static_cast<uint8_t>(v >> 8);
    p[2] = static_cast<uint8_t>(v >> 16);
    p[3] = static_cast<uint8_t>(v >> 24);
}

void store64(uint8_t* p, uint64_t v) {
    store32(p, static_cast<uint32_t>(v));
    store32(p + 4, static_cast<uint32_t>(v >> 32));
}

uint32_t rotl(uint32_t v, int c) { return (v << c) | (v >> (32 - c)); }

void quarterRound(uint32_t& a, uint32_t& b, uint32_t& c, uint32_t& d) {
    a += b; d ^= a; d = rotl(d, 16);
    c += d; b ^= c; b = rotl(b, 12);
    a += b; d ^= a; d = rotl(d, 8);
    c += d; b ^= c; b = rotl(b, 7);
}

void chachaBlock(const uint8_t* key, uint32_t counter, const uint8_t* nonce, uint8_t out[64]) {
    uint32_t s[16] = {0x61707865, 0x3320646e, 0x79622d32, 0x6b206574};
    for (int i = 0; i < 8; ++i) s[4 + i] = load32(key + 4 * i);
    s[12] = counter;
    s[13] = load32(nonce);
    s[14] = load32(nonce + 4);
    s[15] = load32(nonce + 8);

    uint32_t x[16];
    for (int i = 0; i < 16; ++i) x[i] = s[i];
    for (int i = 0; i < 10; ++i) {
        quarterRound(x[0], x[4], x[8], x[12]);
        quarterRound(x[1], x[5], x[9], x[13]);
        quarterRound(x[2], x[6], x[10], x[14]);
        quarterRound(x[3], x[7], x[11], x[15]);
        quarterRound(x[0], x[5], x[10], x[15]);
        quarterRound(x[1], x[6], x[11], x[12]);
        quarterRound(x[2], x[7], x[8], x[13]);
        quarterRound(x[3], x[4], x[9], x[14]);
    }
    for (int i = 0; i < 16; ++i) store32(out + 4 * i, x[i] + s[i]);
}

// XORs data with the ChaCha20 key stream, starting at block `counter`.
void chachaXor(const uint8_t* key, const uint8_t* nonce, uint32_t counter,
               const uint8_t* in, uint8_t* out, std::size_t len) {
    uint8_t block[64];
    for (std::size_t off = 0; off < len; off += 64, ++counter) {
        chachaBlock(key, counter, nonce, block);
        const std::size_t n = (len - off < 64) ? len - off : 64;
        for (std::size_t i = 0; i < n; ++i) out[off + i] = in[off + i] ^ block[i];
    }
}

// ------------------------------------------------------------ Poly1305 (RFC 8439 2.5)
// 26-bit limb implementation. `data` must be a multiple of 16 bytes, which is
// always the case for the AEAD construction (everything is zero padded).
void poly1305(const uint8_t key[32], const std::vector<uint8_t>& data, uint8_t tag[16]) {
    const uint32_t mask = 0x3ffffff;
    const uint32_t r0 = load32(key + 0) & 0x3ffffff;
    const uint32_t r1 = (load32(key + 3) >> 2) & 0x3ffff03;
    const uint32_t r2 = (load32(key + 6) >> 4) & 0x3ffc0ff;
    const uint32_t r3 = (load32(key + 9) >> 6) & 0x3f03fff;
    const uint32_t r4 = (load32(key + 12) >> 8) & 0x00fffff;
    const uint32_t s1 = r1 * 5, s2 = r2 * 5, s3 = r3 * 5, s4 = r4 * 5;

    uint32_t h0 = 0, h1 = 0, h2 = 0, h3 = 0, h4 = 0;
    for (std::size_t off = 0; off < data.size(); off += 16) {
        const uint8_t* m = data.data() + off;
        h0 += load32(m + 0) & mask;
        h1 += (load32(m + 3) >> 2) & mask;
        h2 += (load32(m + 6) >> 4) & mask;
        h3 += (load32(m + 9) >> 6) & mask;
        h4 += (load32(m + 12) >> 8) | (1u << 24);

        const uint64_t d0 = (uint64_t)h0 * r0 + (uint64_t)h1 * s4 + (uint64_t)h2 * s3 + (uint64_t)h3 * s2 + (uint64_t)h4 * s1;
        uint64_t d1 = (uint64_t)h0 * r1 + (uint64_t)h1 * r0 + (uint64_t)h2 * s4 + (uint64_t)h3 * s3 + (uint64_t)h4 * s2;
        uint64_t d2 = (uint64_t)h0 * r2 + (uint64_t)h1 * r1 + (uint64_t)h2 * r0 + (uint64_t)h3 * s4 + (uint64_t)h4 * s3;
        uint64_t d3 = (uint64_t)h0 * r3 + (uint64_t)h1 * r2 + (uint64_t)h2 * r1 + (uint64_t)h3 * r0 + (uint64_t)h4 * s4;
        uint64_t d4 = (uint64_t)h0 * r4 + (uint64_t)h1 * r3 + (uint64_t)h2 * r2 + (uint64_t)h3 * r1 + (uint64_t)h4 * r0;

        uint32_t c = static_cast<uint32_t>(d0 >> 26); h0 = static_cast<uint32_t>(d0) & mask;
        d1 += c; c = static_cast<uint32_t>(d1 >> 26); h1 = static_cast<uint32_t>(d1) & mask;
        d2 += c; c = static_cast<uint32_t>(d2 >> 26); h2 = static_cast<uint32_t>(d2) & mask;
        d3 += c; c = static_cast<uint32_t>(d3 >> 26); h3 = static_cast<uint32_t>(d3) & mask;
        d4 += c; c = static_cast<uint32_t>(d4 >> 26); h4 = static_cast<uint32_t>(d4) & mask;
        h0 += c * 5; c = h0 >> 26; h0 &= mask;
        h1 += c;
    }

    // Fully carry h
    uint32_t c = h1 >> 26; h1 &= mask;
    h2 += c; c = h2 >> 26; h2 &= mask;
    h3 += c; c = h3 >> 26; h3 &= mask;
    h4 += c; c = h4 >> 26; h4 &= mask;
    h0 += c * 5; c = h0 >> 26; h0 &= mask;
    h1 += c;

    // Compute h - p and select it if h >= p
    uint32_t g0 = h0 + 5; c = g0 >> 26; g0 &= mask;
    uint32_t g1 = h1 + c; c = g1 >> 26; g1 &= mask;
    uint32_t g2 = h2 + c; c = g2 >> 26; g2 &= mask;
    uint32_t g3 = h3 + c; c = g3 >> 26; g3 &= mask;
    uint32_t g4 = h4 + c - (1u << 26);

    uint32_t select = (g4 >> 31) - 1;   // all ones if h >= p
    g0 &= select; g1 &= select; g2 &= select; g3 &= select; g4 &= select;
    select = ~select;
    h0 = (h0 & select) | g0;
    h1 = (h1 & select) | g1;
    h2 = (h2 & select) | g2;
    h3 = (h3 & select) | g3;
    h4 = (h4 & select) | g4;

    // h = h % 2^128, then add the second half of the key
    h0 = h0 | (h1 << 26);
    h1 = (h1 >> 6) | (h2 << 20);
    h2 = (h2 >> 12) | (h3 << 14);
    h3 = (h3 >> 18) | (h4 << 8);

    uint64_t f = (uint64_t)h0 + load32(key + 16);              h0 = static_cast<uint32_t>(f);
    f = (uint64_t)h1 + load32(key + 20) + (f >> 32);           h1 = static_cast<uint32_t>(f);
    f = (uint64_t)h2 + load32(key + 24) + (f >> 32);           h2 = static_cast<uint32_t>(f);
    f = (uint64_t)h3 + load32(key + 28) + (f >> 32);           h3 = static_cast<uint32_t>(f);

    store32(tag + 0, h0);
    store32(tag + 4, h1);
    store32(tag + 8, h2);
    store32(tag + 12, h3);
}

// ------------------------------------------------------------ AEAD (RFC 8439 2.8)

void pad16(std::vector<uint8_t>& v) {
    while (v.size() % 16 != 0) v.push_back(0);
}

void computeTag(const uint8_t* key, const uint8_t* nonce,
                const uint8_t* aad, std::size_t aadLen,
                const uint8_t* ct, std::size_t ctLen, uint8_t tag[16]) {
    uint8_t block0[64];
    chachaBlock(key, 0, nonce, block0);   // first 32 bytes are the one-time Poly1305 key

    std::vector<uint8_t> macData(aad, aad + aadLen);
    pad16(macData);
    macData.insert(macData.end(), ct, ct + ctLen);
    pad16(macData);
    uint8_t lengths[16];
    store64(lengths, aadLen);
    store64(lengths + 8, ctLen);
    macData.insert(macData.end(), lengths, lengths + 16);

    poly1305(block0, macData, tag);
}

// Seal / open with an explicit nonce. Output / input layout: nonce|ct|tag.
std::vector<uint8_t> chachaPolySeal(const std::vector<uint8_t>& key, const uint8_t* nonce,
                                    const std::vector<uint8_t>& aad,
                                    const std::vector<uint8_t>& plaintext) {
    std::vector<uint8_t> out(kNonceSize + plaintext.size() + kTagSize);
    std::copy(nonce, nonce + kNonceSize, out.begin());
    uint8_t* ct = out.data() + kNonceSize;
    chachaXor(key.data(), nonce, 1, plaintext.data(), ct, plaintext.size());
    computeTag(key.data(), nonce, aad.data(), aad.size(), ct, plaintext.size(),
               out.data() + kNonceSize + plaintext.size());
    return out;
}

std::vector<uint8_t> chachaPolyOpen(const std::vector<uint8_t>& key,
                                    const std::vector<uint8_t>& aad,
                                    const std::vector<uint8_t>& packet) {
    if (packet.size() < kNonceSize + kTagSize) {
        throw std::runtime_error("Encrypted packet is too short.");
    }
    const uint8_t* nonce = packet.data();
    const uint8_t* ct = packet.data() + kNonceSize;
    const std::size_t ctLen = packet.size() - kNonceSize - kTagSize;

    uint8_t expected[kTagSize];
    computeTag(key.data(), nonce, aad.data(), aad.size(), ct, ctLen, expected);
    if (!constantTimeEqual(expected, ct + ctLen, kTagSize)) {
        throw std::runtime_error("Encrypted packet failed authentication.");
    }

    std::vector<uint8_t> plaintext(ctLen);
    chachaXor(key.data(), nonce, 1, ct, plaintext.data(), ctLen);
    return plaintext;
}

} // namespace

// ------------------------------------------------------------ ChaCha20Poly1305Strategy

std::vector<uint8_t> ChaCha20Poly1305Strategy::encrypt(const std::vector<uint8_t>& plaintext,
                                                       const std::vector<uint8_t>& key) {
    requireKeySize(key);
    const std::vector<uint8_t> nonce = randomBytes(kNonceSize);
    return chachaPolySeal(key, nonce.data(), {}, plaintext);
}

std::vector<uint8_t> ChaCha20Poly1305Strategy::decrypt(const std::vector<uint8_t>& ciphertext,
                                                       const std::vector<uint8_t>& key) {
    requireKeySize(key);
    return chachaPolyOpen(key, {}, ciphertext);
}

// ------------------------------------------------------------ AES256GCMStrategy

#ifdef HMS_HAVE_OPENSSL
namespace {
struct CtxDeleter {
    void operator()(EVP_CIPHER_CTX* c) const { EVP_CIPHER_CTX_free(c); }
};
using CtxPtr = std::unique_ptr<EVP_CIPHER_CTX, CtxDeleter>;
} // namespace

std::vector<uint8_t> AES256GCMStrategy::encrypt(const std::vector<uint8_t>& plaintext,
                                                const std::vector<uint8_t>& key) {
    requireKeySize(key);
    const std::vector<uint8_t> nonce = randomBytes(kNonceSize);
    std::vector<uint8_t> out(kNonceSize + plaintext.size() + kTagSize);
    std::copy(nonce.begin(), nonce.end(), out.begin());

    CtxPtr ctx(EVP_CIPHER_CTX_new());
    int len = 0;
    if (!ctx ||
        EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, kNonceSize, nullptr) != 1 ||
        EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr, key.data(), nonce.data()) != 1 ||
        EVP_EncryptUpdate(ctx.get(), out.data() + kNonceSize, &len, plaintext.data(),
                          static_cast<int>(plaintext.size())) != 1 ||
        EVP_EncryptFinal_ex(ctx.get(), out.data() + kNonceSize + len, &len) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, kTagSize,
                            out.data() + kNonceSize + plaintext.size()) != 1) {
        throw std::runtime_error("AES-256-GCM encryption failed.");
    }
    return out;
}

std::vector<uint8_t> AES256GCMStrategy::decrypt(const std::vector<uint8_t>& ciphertext,
                                                const std::vector<uint8_t>& key) {
    requireKeySize(key);
    if (ciphertext.size() < kNonceSize + kTagSize) {
        throw std::runtime_error("Encrypted packet is too short.");
    }
    const std::size_t ctLen = ciphertext.size() - kNonceSize - kTagSize;
    std::vector<uint8_t> plaintext(ctLen);
    std::vector<uint8_t> tag(ciphertext.end() - kTagSize, ciphertext.end());

    CtxPtr ctx(EVP_CIPHER_CTX_new());
    int len = 0;
    if (!ctx ||
        EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, kNonceSize, nullptr) != 1 ||
        EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr, key.data(), ciphertext.data()) != 1 ||
        EVP_DecryptUpdate(ctx.get(), plaintext.data(), &len, ciphertext.data() + kNonceSize,
                          static_cast<int>(ctLen)) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, kTagSize, tag.data()) != 1 ||
        EVP_DecryptFinal_ex(ctx.get(), plaintext.data() + len, &len) != 1) {
        throw std::runtime_error("Encrypted packet failed authentication.");
    }
    return plaintext;
}
#endif

// ------------------------------------------------------------ EncryptionManager

EncryptionManager::EncryptionManager(std::unique_ptr<IEncryptionStrategy> strategy, std::vector<uint8_t> masterKey)
    : strategy_(std::move(strategy)), masterKey_(std::move(masterKey)) {
    if (!strategy_) throw std::invalid_argument("No encryption strategy given.");
    requireKeySize(masterKey_);
}

EncryptionManager::~EncryptionManager() {
    // Wipe the key so it does not linger in freed memory.
    volatile uint8_t* p = masterKey_.data();
    for (std::size_t i = 0; i < masterKey_.size(); ++i) p[i] = 0;
}

std::vector<uint8_t> EncryptionManager::decryptADCMessage(const std::vector<uint8_t>& encryptedPacket) {
    if (encryptedPacket.empty()) {
        throw std::invalid_argument("Encrypted packet is empty.");
    }
    // Delegates decryption to the hidden strategy
    return strategy_->decrypt(encryptedPacket, masterKey_);
}

std::vector<uint8_t> EncryptionManager::encryptForReport(const std::vector<uint8_t>& reportData) {
    if (reportData.empty()) {
        throw std::invalid_argument("Report data is empty.");
    }
    // Delegates encryption before handing off to report generator / storage
    return strategy_->encrypt(reportData, masterKey_);
}

std::vector<uint8_t> EncryptionManager::encryptForADC(const std::vector<uint8_t>& message) {
    if (message.empty()) {
        throw std::invalid_argument("Message is empty.");
    }
    return strategy_->encrypt(message, masterKey_);
}

std::unique_ptr<IEncryptionStrategy> EncryptionManager::defaultStrategy() {
#ifdef HMS_HAVE_OPENSSL
    return std::make_unique<AES256GCMStrategy>();
#else
    return std::make_unique<ChaCha20Poly1305Strategy>();
#endif
}

std::vector<uint8_t> EncryptionManager::generateKey() {
    return randomBytes(kKeySize);
}
