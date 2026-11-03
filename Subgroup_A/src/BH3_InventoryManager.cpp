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
            if (timer > 30) {
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

    int RequestDrug(std::string drugname, int dose, std::string unit) {
        if (currentState != SystemState::Dispensation_Mode) {
            return -1; 
        }
        
        int stock_available = SW1::get_drug_amount(drugname);
        if (stock_available < dose) {
            HW2::display_text("Error: Insufficient Stock");
            return -2; 
        }
        
        if (HW0::count_open_drawers() > 0) {
            HW2::display_text("Error: Close open drawer first");
            return -3; 
        }

        int drawer_id = std::stoi(SW1::get_drug_location(drugname)); 
        HW0::Open_Drawer(drawer_id);
        BH4::generate_Event("Dispensed " + std::to_string(dose) + " of " + drugname);
        HW2::display_text("Please remove medication and close drawer.");

        std::thread safetyThread(MonitorDrawerSafety, drawer_id);
        safetyThread.detach();

        return 0; 
    }

    int addDrug(std::string drugname, int amount, std::string unit, int drawerID) {
        SW1::add_drug(drugname, amount, drawerID);
        return 0;
    }
}