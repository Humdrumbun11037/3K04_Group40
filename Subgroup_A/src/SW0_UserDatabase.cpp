#include "SW0_UserDatabase.hpp"
#include <unordered_map>

namespace SW0 {
    // Hidden Secret: Database implementation format
    static std::unordered_map<std::string, std::pair<std::string, std::string>> users = {
        {"nurse_jane", {"hash123", "Nurse"}},
        {"pharm_bob", {"hash456", "Pharmacist"}}
    };

    std::string Check_user(std::string name, std::string password_hash) {
        if (users.find(name) != users.end() && users[name].first == password_hash) {
            return users[name].second; 
        }
        return "Invalid";
    }

    std::string add_user(std::string name, std::string password_hash, std::string admin_name, std::string admin_hash) {
        if (Check_user(admin_name, admin_hash) == "Admin") {
            users[name] = {password_hash, "Nurse"};
            return "Success";
        }
        return "Denied";
    }

    std::string remove_user(std::string name, std::string password_hash, std::string admin_name, std::string admin_hash) {
        if (Check_user(admin_name, admin_hash) == "Admin") {
            users.erase(name);
            return "Success";
        }
        return "Denied";
    }
}