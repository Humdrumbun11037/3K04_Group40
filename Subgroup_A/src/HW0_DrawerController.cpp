#include "HW0_DrawerController.hpp"
#include <iostream>

namespace HW0 {
    int Open_Drawer(int drawer_id) {
        // Disengages the mechanical lock and outputs signal to motor[cite: 8, 9]
        std::cout << "[HW-0] Actuator engaged: Drawer " << drawer_id << " OPENED.\n";
        return 1; // 1 represents flawless execution[cite: 9]
    }
    
    int Close_Drawer(int drawer_id) {
        // Engages the mechanical lock[cite: 9]
        if (drawer_id == -1) {
            std::cout << "[HW-0] FORCE LOCK ENGAGED on all drawers.\n";
        } else {
            std::cout << "[HW-0] Actuator engaged: Drawer " << drawer_id << " CLOSED.\n";
        }
        return 1;
    }
    
    int count_open_drawers() {
        // Stub returning 0 open drawers for testing normal paths
        return 0; 
    }
    
    std::string check_drawer_status(int drawer_id) {
        // Stub returning CLOSED so the safety thread doesn't loop infinitely in basic testing
        return "CLOSED"; 
    }
}