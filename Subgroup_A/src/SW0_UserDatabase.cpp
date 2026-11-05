#include "SW0_UserDatabase.hpp"
#include <unordered_map>

namespace SW0 {
    struct User {
        std::string password;
        std::string authority;
    };
    
    // Default users
    static std::unordered_map<std::string, User> users = {
        {"nurse_jane", {"hash123", "Nurse"}},
        {"pharm_bob", {"hash456", "Pharmacist"}},
        {"tech_tom", {"hash789", "Technician"}}
    };

    std::string Check_user(std::string name, std::string password_hash) {
        if (users.find(name) != users.end() && users[name].password == password_hash) {
            return users[name].authority;
        }
        return "Invalid";
    }

    std::string add_user(std::string name, std::string password_hash, std::string role) {
        if (users.find(name) != users.end()) return "Exists";
        users[name] = {password_hash, role};
        return "Success";
    }

    std::string remove_user(std::string name) {
        if (users.find(name) != users.end()) {
            users.erase(name);
            return "Success";
        }
        return "Not Found";
    }

    std::string get_user_role(std::string name) {
        if (users.find(name) != users.end()) return users[name].authority;
        return "Invalid";
    }

    int count_role(std::string role) {
        int count = 0;
        for (const auto& pair : users) {
            if (pair.second.authority == role) count++;
        }
        return count;
    }
}