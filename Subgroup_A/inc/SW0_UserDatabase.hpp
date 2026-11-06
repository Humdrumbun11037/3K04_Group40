#ifndef SW0_USERDATABASE_HPP
#define SW0_USERDATABASE_HPP
#include <string>

namespace SW0 {
    std::string Check_user(std::string name, std::string password_hash);
    std::string add_user(std::string name, std::string password_hash, std::string role);
    std::string remove_user(std::string name);
    std::string get_user_role(std::string name);
    int count_role(std::string role);
}

#endif