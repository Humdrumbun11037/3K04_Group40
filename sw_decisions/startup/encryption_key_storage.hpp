#ifndef ENCRYPTION_KEY_STORAGE_HPP
#define ENCRYPTION_KEY_STORAGE_HPP

#include <cstdint>
#include <string>
#include <vector>

std::vector<uint8_t> loadOrCreateEncryptionKey(
    const std::string& filename
);

#endif