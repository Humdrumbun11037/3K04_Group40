#ifndef HW1_TRANSCEIVER_HPP
#define HW1_TRANSCEIVER_HPP

#include <string>

namespace HW1 {
    std::string Receive_Message();
    int Send_Message(std::string mess_content);
    int Check_Connection();
}

#endif