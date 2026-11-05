//encryption

/*
Hides: the encryption algorithm, the key, and the layout of an encrypted
       packet

Receives: encrypted packets from message_routing,
          report data from report_generation
Sends: decrypted messages back to message_routing,
       encrypted reports back to report_generation

Encrypted packet layout (both strategies):
    [12 byte nonce][ciphertext][16 byte authentication tag]
*/
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

/**
 * @brief Abstract interface defining the encryption strategy.
 * This allows the system to swap underlying algorithms (e.g., AES-256-GCM,
 * ChaCha20-Poly1305) without changing the rest of the application code.
 *
 * decrypt() throws std::runtime_error if the packet was tampered with or
 * the key is wrong. Both functions throw std::invalid_argument for a key of
 * the wrong size.
 */
class IEncryptionStrategy {
public:
    virtual ~IEncryptionStrategy() = default;

    virtual std::vector<uint8_t> encrypt(
        const std::vector<uint8_t>& plaintext,
        const std::vector<uint8_t>& key
    ) = 0;

    virtual std::vector<uint8_t> decrypt(
        const std::vector<uint8_t>& ciphertext,
        const std::vector<uint8_t>& key
    ) = 0;
};

/**
 * @brief ChaCha20-Poly1305 (RFC 8439). Self-contained, no external library.
 * Used by default. Key: 32 bytes.
 */
class ChaCha20Poly1305Strategy : public IEncryptionStrategy {
public:
    std::vector<uint8_t> encrypt(const std::vector<uint8_t>& plaintext,
                                 const std::vector<uint8_t>& key) override;
    std::vector<uint8_t> decrypt(const std::vector<uint8_t>& ciphertext,
                                 const std::vector<uint8_t>& key) override;
};

#ifdef HMS_HAVE_OPENSSL
/**
 * @brief AES-256-GCM through OpenSSL. Only built when HMS_HAVE_OPENSSL is
 * defined and the program is linked with -lcrypto. Key: 32 bytes.
 */
class AES256GCMStrategy : public IEncryptionStrategy {
public:
    std::vector<uint8_t> encrypt(const std::vector<uint8_t>& plaintext,
                                 const std::vector<uint8_t>& key) override;
    std::vector<uint8_t> decrypt(const std::vector<uint8_t>& ciphertext,
                                 const std::vector<uint8_t>& key) override;
};
#endif

/**
 * @brief Facade class managing encryption and decryption operations for the HMS.
 */
class EncryptionManager {
private:
    std::unique_ptr<IEncryptionStrategy> strategy_;
    std::vector<uint8_t> masterKey_;

public:
    static constexpr std::size_t kKeySize = 32;

    /**
     * @brief Construct a new Encryption Manager with a specific strategy and key.
     */
    EncryptionManager(std::unique_ptr<IEncryptionStrategy> strategy, std::vector<uint8_t> masterKey);

    ~EncryptionManager();

    /**
     * @brief Decrypts an incoming message packet received from an ADC.
     */
    std::vector<uint8_t> decryptADCMessage(const std::vector<uint8_t>& encryptedPacket);

    /**
     * @brief Encrypts data before it gets handed off to report generation or long-term storage.
     */
    std::vector<uint8_t> encryptForReport(const std::vector<uint8_t>& reportData);

    /**
     * @brief Encrypts a message to be sent to an ADC (also used to simulate ADCs in tests).
     */
    std::vector<uint8_t> encryptForADC(const std::vector<uint8_t>& message);

    /**
     * @brief The best strategy available in this build.
     */
    static std::unique_ptr<IEncryptionStrategy> defaultStrategy();

    /**
     * @brief A new random key of kKeySize bytes.
     */
    static std::vector<uint8_t> generateKey();

    std::vector<uint8_t> encryptData(
        const std::vector<uint8_t>& plaintext
    );
 
    std::vector<uint8_t> decryptData(
        const std::vector<uint8_t>& ciphertext
    );
};
