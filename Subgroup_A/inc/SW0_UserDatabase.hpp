#ifndef SW0_USER_DATABASE_HPP
#define SW0_USER_DATABASE_HPP

#include <string>

namespace SW0 {
    std::string Check_user(std::string name, std::string password_hash);
    std::string add_user(std::string name, std::string password_hash, std::string admin_name, std::string admin_hash);
    std::string remove_user(std::string name, std::string password_hash, std::string admin_name, std::string admin_hash);
}

#endif