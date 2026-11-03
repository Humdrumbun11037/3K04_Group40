#include "BH1_RequestHandler.hpp"
#include "HW2_DisplayController.hpp"

namespace BH1 {
    std::string request(std::string request_name, std::string user_class) {
        if (user_class == "Valid") {
            return "Authorized";
        } else {
            HW2::display_text("Unauthorized Request");
            return "Denied";
        }
    }
}