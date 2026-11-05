#include "BH3_InventoryManager.hpp"
#include "SW1_InventoryDatabase.hpp"
#include "HW0_DrawerController.hpp"
#include "HW2_DisplayController.hpp"
#include "BH4_EventLogger.hpp"
#include "BH5_AlarmHandler.hpp"
#include "SystemModes.hpp"
#include <thread>
#include <chrono>

namespace BH3 {
    void MonitorDrawerSafety(int drawer_id) {
        int timer = 0;
        while (HW0::check_drawer_status(drawer_id) == "OPEN") {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            timer++;
            if (timer > 30) { // Change to 3 for faster manual testing
                HW0::Close_Drawer(drawer_id);
                BH5::alarm("TIMEOUT_FAULT");
                currentState = SystemState::Fault_Mode;
                HW2::display_text("SYSTEM LOCKED: DRAWER TIMEOUT. CONTACT ADMIN.");
                BH4::generate_Event("CRITICAL FAULT: Drawer " + std::to_string(drawer_id) + " left open.");
                break;
            }
        }
        if (currentState != SystemState::Fault_Mode) {
            HW0::Close_Drawer(drawer_id);
            BH4::generate_Event("Drawer " + std::to_string(drawer_id) + " safely secured.");
        }
    }

    int RequestDrug(std::string drugname, int dose, std::string unit, std::string username) {        
        (void)unit; // Silence compiler warning
        
        if (currentState != SystemState::Dispensation_Mode) {
            return -1; 
        }
        if (dose <= 0) { 
            HW2::display_text("Error: Dispense amount must be greater than 0.");
            return -5;
        }
        
        std::string loc = SW1::get_drug_location(drugname);
        if (loc == "-1") {
            HW2::display_text("Error: " + drugname + " not found in inventory.");
            return -4;
        }

        int stock_available = SW1::get_drug_amount(drugname);
        if (stock_available < dose) {
            HW2::display_text("Error: Insufficient Stock. Only " + std::to_string(stock_available) + " remaining.");
            return -2; 
        }
        
        if (HW0::count_open_drawers() > 0) {
            HW2::display_text("Error: Close open drawer first");
            return -3; 
        }

        int drawer_id = std::stoi(loc); 
        HW0::Open_Drawer(drawer_id);
        
        SW1::remove_drug(drugname, dose, drawer_id);
        
        BH4::generate_Event(username + " dispensed " + std::to_string(dose) + "mg of " + drugname);        
        
        int remaining = SW1::get_drug_amount(drugname);
        HW2::display_text("Dispensed. Please close drawer. Remaining " + drugname + ": " + std::to_string(remaining));

        std::thread safetyThread(MonitorDrawerSafety, drawer_id);
        safetyThread.detach();

        return 0; 
    }

    int addDrug(std::string drugname, int amount, std::string unit, int drawerID, std::string username) {        
        (void)unit; // Silence compiler warning
        if (amount <= 0) return -1; // 
        SW1::add_drug(drugname, amount, drawerID);
        BH4::generate_Event(username + " restocked " + std::to_string(amount) + "mg of " + drugname);
        return 0;
    }
    std::string getAvailableMedicines() {
        return SW1::get_all_inventory();
    }
}