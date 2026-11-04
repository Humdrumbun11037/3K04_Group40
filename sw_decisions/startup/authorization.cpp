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

#include "authorization.hpp"

Authorization::Authorization(UserRegistry& users, const UserSettings& settings,
                             EventRecorder& recorder, const Clock& clock)
    : users_(users), settings_(settings), recorder_(recorder), clock_(clock) {}

Authorization::LoginResult Authorization::login(const std::string& userId,
                                                const std::string& password) {
    if (users_.isAccountLocked(userId)) {
        logAttempt(userId, "rejected: account locked");
        return LoginResult::AccountLocked;
    }

    if (!users_.authenticate(userId, password)) {
        // authenticate() may have just locked the account (4th failure)
        if (users_.isAccountLocked(userId)) {
            logAttempt(userId, "failed: account now locked");
            return LoginResult::AccountLocked;
        }
        // Same answer whether the id or the password was wrong, so the
        // response does not reveal which user ids exist.
        logAttempt(userId, "failed");
        return LoginResult::InvalidCredentials;
    }

    const std::string now = clock_.now();
    sessions_[userId] = Session{userId, users_.getUserRole(userId), now, now};
    logAttempt(userId, "success");
    return LoginResult::Success;
}

void Authorization::logout(const std::string& userId) {
    sessions_.erase(userId);
}

Session* Authorization::liveSession(const std::string& userId) {
    auto it = sessions_.find(userId);
    if (it == sessions_.end()) return nullptr;

    // The account may have been removed or locked since the user logged in.
    if (!users_.exists(userId) || users_.isAccountLocked(userId) ||
        timeutil::secondsBetween(it->second.lastActivity, clock_.now()) > kSessionTimeoutSeconds) {
        sessions_.erase(it);
        return nullptr;
    }
    return &it->second;
}

std::optional<Session> Authorization::session(const std::string& userId) {
    Session* s = liveSession(userId);
    if (s == nullptr) return std::nullopt;
    return *s;
}

bool Authorization::isLoggedIn(const std::string& userId) {
    return liveSession(userId) != nullptr;
}

bool Authorization::authorize(const std::string& userId, Permission permission) {
    Session* s = liveSession(userId);
    if (s == nullptr) return false;
    s->lastActivity = clock_.now();
    return settings_.isAllowed(s->role, permission);
}

std::string Authorization::describe(LoginResult result) {
    switch (result) {
        case LoginResult::Success:            return "Login successful.";
        case LoginResult::InvalidCredentials: return "Incorrect user id or password.";
        case LoginResult::AccountLocked:
            return "Account is locked after too many failed attempts. Contact an administrator.";
    }
    return "";
}

void Authorization::logAttempt(const std::string& userId, const std::string& result) {
    Event event;
    event.type = EventType::UserLogin;
    event.timestamp = clock_.now();
    event.fields["user"] = userId;
    event.fields["result"] = result;
    recorder_.record(event);
}
