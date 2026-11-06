#include "encryption_key_storage.hpp"
#include "../message_processing/encryption.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>

std::vector<uint8_t> loadOrCreateEncryptionKey(
    const std::string& filename
)
{
    namespace fs = std::filesystem;

    const std::size_t keySize = EncryptionManager::kKeySize;

    // Create the parent directory if necessary.
    fs::path keyPath(filename);
    fs::path parent = keyPath.parent_path();

    if (!parent.empty()) {
        fs::create_directories(parent);
    }

    // If a key already exists, load it.
    if (fs::exists(keyPath)) {

        std::ifstream input(filename, std::ios::binary);

        if (!input) {
            throw std::runtime_error(
                "Could not open existing encryption key."
            );
        }

        std::vector<uint8_t> key(keySize);

        input.read(
            reinterpret_cast<char*>(key.data()),
            static_cast<std::streamsize>(keySize)
        );

        if (input.gcount() != static_cast<std::streamsize>(keySize)) {
            throw std::runtime_error(
                "Existing encryption key has an invalid size."
            );
        }

        // Reject files containing extra bytes too.
        if (input.peek() != std::ifstream::traits_type::eof()) {
            throw std::runtime_error(
                "Existing encryption key contains unexpected extra data."
            );
        }

        return key;
    }

    // No key exists: generate one and save it.
    std::vector<uint8_t> key = EncryptionManager::generateKey();

    std::ofstream output(
        filename,
        std::ios::binary | std::ios::out
    );

    if (!output) {
        throw std::runtime_error(
            "Could not create encryption key file."
        );
    }

    output.write(
        reinterpret_cast<const char*>(key.data()),
        static_cast<std::streamsize>(key.size())
    );

    output.flush();

    if (!output) {
        throw std::runtime_error(
            "Failed to save encryption key."
        );
    }

    return key;
}