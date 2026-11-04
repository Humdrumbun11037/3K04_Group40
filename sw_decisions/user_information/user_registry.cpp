//user registry

/*
Hides: how user accounts and credentials are stored, and the account lockout
       rule

Receives: user management commands from the administrator's dashboard,
          login attempts from authorization
Sends: account information (role, lock status, users of each role) to
       authorization and alarm_notification
*/

#include "user_registry.hpp"

#include <algorithm>
#include <cstdint>
#include <random>

bool UserRegistry::addUser(const std::string& userId, const std::string& password, UserRole role) {
    if (userId.empty() || password.empty()) {
        return false;
    }
    if (accounts_.find(userId) != accounts_.end()) {
        return false; // User already exists (Duplicate ID check)
    }

    UserAccount newAccount;
    newAccount.userId = userId;
    newAccount.salt = makeSalt();
    newAccount.passwordHash = hashPassword(password, newAccount.salt);
    newAccount.role = role;
    newAccount.isLocked = false;
    newAccount.failedLoginAttempts = 0;

    accounts_[userId] = newAccount;
    return true;
}

bool UserRegistry::removeUser(const std::string& userId) {
    return accounts_.erase(userId) > 0;
}

bool UserRegistry::authenticate(const std::string& userId, const std::string& password) {
    auto it = accounts_.find(userId);
    if (it == accounts_.end()) {
        return false; // User does not exist
    }

    UserAccount& account = it->second;

    if (account.isLocked) {
        return false; // Account is locked (FR-USR-03 / Rule 4)
    }

    if (account.passwordHash == hashPassword(password, account.salt)) {
        account.failedLoginAttempts = 0; // Reset on successful login
        return true;
    } else {
        account.failedLoginAttempts++;
        if (account.failedLoginAttempts >= kMaxFailedAttempts) {
            account.isLocked = true; // Lock after 4 consecutive failures (FR-USR-03)
        }
        return false;
    }
}

bool UserRegistry::exists(const std::string& userId) const {
    return accounts_.find(userId) != accounts_.end();
}

bool UserRegistry::isAccountLocked(const std::string& userId) const {
    auto it = accounts_.find(userId);
    if (it != accounts_.end()) {
        return it->second.isLocked;
    }
    return false;
}

bool UserRegistry::unlockAccount(const std::string& userId) {
    auto it = accounts_.find(userId);
    if (it == accounts_.end()) {
        return false;
    }
    it->second.isLocked = false;
    it->second.failedLoginAttempts = 0;
    return true;
}

bool UserRegistry::changePassword(const std::string& userId,
                                  const std::string& oldPassword,
                                  const std::string& newPassword) {
    if (newPassword.empty() || !authenticate(userId, oldPassword)) {
        return false;
    }
    UserAccount& account = accounts_.at(userId);
    account.salt = makeSalt();
    account.passwordHash = hashPassword(newPassword, account.salt);
    return true;
}

UserRole UserRegistry::getUserRole(const std::string& userId) const {
    auto it = accounts_.find(userId);
    if (it != accounts_.end()) {
        return it->second.role;
    }
    throw std::runtime_error("User not found");
}

std::vector<std::string> UserRegistry::usersWithRole(UserRole role) const {
    std::vector<std::string> ids;
    for (const auto& entry : accounts_) {
        if (entry.second.role == role) {
            ids.push_back(entry.first);
        }
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::string UserRegistry::makeSalt() {
    static const char hex[] = "0123456789abcdef";
    std::random_device rd;
    std::string salt;
    for (int i = 0; i < 4; ++i) {
        uint32_t r = rd();
        for (int j = 0; j < 8; ++j) {
            salt += hex[(r >> (4 * j)) & 0xF];
        }
    }
    return salt;
}

// Salted, iterated FNV-1a so plain-text passwords are never stored.
// NOTE: this is a placeholder. Before deployment, replace it with a proper
// password hashing function (Argon2id, bcrypt or scrypt); only this function
// needs to change.
std::string UserRegistry::hashPassword(const std::string& password, const std::string& salt) {
    const uint64_t kOffset = 1469598103934665603ULL;
    const uint64_t kPrime = 1099511628211ULL;
    const std::string input = salt + ":" + password;

    uint64_t h = kOffset;
    for (int round = 0; round < 10000; ++round) {
        for (unsigned char c : input) {
            h ^= c;
            h *= kPrime;
        }
        h ^= static_cast<uint64_t>(round);
        h *= kPrime;
    }

    static const char hex[] = "0123456789abcdef";
    std::string out(16, '0');
    for (int i = 15; i >= 0; --i) {
        out[i] = hex[h & 0xF];
        h >>= 4;
    }
    return out;
}
