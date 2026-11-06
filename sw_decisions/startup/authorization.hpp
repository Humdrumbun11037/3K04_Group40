//authorization

/*
Hides: how users log in to the HMS, how sessions are kept, and how each
       requested action is checked against the user's role

Receives: login / logout requests and user actions from the user dashboard,
          account information from user_registry,
          role permissions from user_settings
Sends: whether a login or an action is allowed,
       login attempts to report_generation
*/
#pragma once

#include <optional>
#include <string>
#include <unordered_map>

#include "../external_interfaces.hpp"
#include "../report_generation.hpp"
#include "../user_information/user_registry.hpp"
#include "../user_information/user_settings.hpp"

struct Session {
    std::string userId;
    UserRole role;
    std::string loginTime;
    std::string lastActivity;
};

class Authorization {
public:
    // A session with no activity for this long is ended automatically.
    static constexpr long long kSessionTimeoutSeconds = 15 * 60;

    Authorization(UserRegistry& users, const UserSettings& settings,
                  EventRecorder& recorder, const Clock& clock);

    enum class LoginResult { Success, InvalidCredentials, AccountLocked };

    LoginResult login(const std::string& userId, const std::string& password);
    void logout(const std::string& userId);

    // The user's session, or nullopt if they are not logged in or it expired.
    std::optional<Session> session(const std::string& userId);
    bool isLoggedIn(const std::string& userId);

    // True if the user is logged in and their role allows the action.
    // Counts as activity, so it keeps the session alive.
    bool authorize(const std::string& userId, Permission permission);

    static std::string describe(LoginResult result);

private:
    // Removes the session if it has timed out. Returns the live session or nullptr.
    Session* liveSession(const std::string& userId);
    void logAttempt(const std::string& userId, const std::string& result);

    UserRegistry& users_;
    const UserSettings& settings_;
    EventRecorder& recorder_;
    const Clock& clock_;
    std::unordered_map<std::string, Session> sessions_;
};
