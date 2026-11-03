#include "HW3_AlarmExecutor.hpp"
#include <iostream>

namespace HW3 {
    int start_strobe() {
        // Toggles the LED lights[cite: 10]
        std::cout << "[HW-3] ALARM STROBE FLASHING! (High voltage signal sent to LED)\n";
        return 1;
    }
    
    int stop_strobe() {
        std::cout << "[HW-3] Alarm strobe disabled.\n";
        return 1;
    }
    
    int start_beep() {
        // Toggles the piezo speaker[cite: 10]
        std::cout << "[HW-3] BEEP! BEEP! BEEP! (High voltage signal sent to speaker)\n";
        return 1;
    }
    
    int stop_beep() {
        std::cout << "[HW-3] Alarm speaker disabled.\n";
        return 1;
    }
    
    int alert_security() {
        // Triggers the relay connected to the hospital security network[cite: 10]
        std::cout << "[HW-3] CRITICAL: Security Relay triggered! Hospital Staff Notified!\n";
        return 1;
    }
}