#include "HW1_Transceiver.hpp"
#include <iostream>

namespace HW1 {
    std::string Receive_Message() {
        // Reads the physical communication line buffer[cite: 9]
        std::cout << "[HW-1] Reading HMS receiver buffer...\n";
        return "HMS_ACK";
    }
    
    int Send_Message(std::string mess_content) {
        // Converts string into network signals and transmits to HMS[cite: 9]
        std::cout << "[HW-1] Transmitting to HMS: " << mess_content << "\n";
        return 1;
    }
    
    int Check_Connection() {
        // Polls to verify response from the HMS[cite: 9]
        std::cout << "[HW-1] Polling HMS connection...\n";
        return 1; 
    }
}