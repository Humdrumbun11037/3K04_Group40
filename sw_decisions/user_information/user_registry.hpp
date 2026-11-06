//user registry

/*
Hides: how user accounts and credentials are stored, and the account lockout
       rule

Receives: user management commands from the administrator's dashboard,
          login attempts from authorization
Sends: account information (role, lock status, users of each role) to
       authorization and alarm_notification
*/
#pragma once

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "user_settings.hpp"

struct UserAccount {
    std::string userId;
    std::string salt;
    std::string passwordHash;
    UserRole role = UserRole::Technician;
    bool isLocked = false;
    int failedLoginAttempts = 0;
};

class UserRegistry {
public:
    // FR-USR-03: an account is locked after this many consecutive failures
    static constexpr int kMaxFailedAttempts = 4;

    // Returns false if the id is taken or the id / password is empty.
    bool addUser(const std::string& userId, const std::string& password, UserRole role);
    bool removeUser(const std::string& userId);

    // Checks the password, counts failures and locks the account if needed.
    bool authenticate(const std::string& userId, const std::string& password);

    bool exists(const std::string& userId) const;
    bool isAccountLocked(const std::string& userId) const;
    bool unlockAccount(const std::string& userId);
    bool changePassword(const std::string& userId,
                        const std::string& oldPassword,
                        const std::string& newPassword);

    // Throws std::runtime_error if the user does not exist.
    UserRole getUserRole(const std::string& userId) const;

    // Ids of every user with the given role, sorted.
    std::vector<std::string> usersWithRole(UserRole role) const;

private:
    std::unordered_map<std::string, UserAccount> accounts_;

    static std::string makeSalt();
    static std::string hashPassword(const std::string& password, const std::string& salt);
};
