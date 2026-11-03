#include "BH2_LoginAuthenticator.hpp"
#include "SW0_UserDatabase.hpp"
#include "BH5_AlarmHandler.hpp"
#include "SystemModes.hpp"

namespace BH2 {
    static int failed_attempts = 0;

    std::string check_user(std::string username, std::string password) {
        std::string authority = SW0::Check_user(username, password); 
        
        if (authority != "Invalid") {
            failed_attempts = 0;
            return authority;
        } else {
            if (failed_attempts < 2) {
                failed_attempts++;
                return "Invalid";
            } else {
                failed_attempts++;
                currentState = SystemState::Security_Lockout;
                BH5::alarm("INVALID_LOGIN_MAX");
                return "Invalid";
            }
        }
    }
}