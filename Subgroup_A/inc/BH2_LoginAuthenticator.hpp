#ifndef BH2_LOGIN_AUTHENTICATOR_HPP
#define BH2_LOGIN_AUTHENTICATOR_HPP

#include <string>

namespace BH2 {
    std::string check_user(std::string username, std::string password);
    void reset_attempts();
}

#endif