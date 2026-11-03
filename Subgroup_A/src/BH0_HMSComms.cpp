#include "BH0_HMSComms.hpp"
#include "HW1_Transceiver.hpp"
#include "BH5_AlarmHandler.hpp"
#include "SystemModes.hpp"

namespace BH0 {
    std::string send_message() {
        HW1::Send_Message("Status Update");
        return "Success";
    }
    
    int verify_connection() {
        if (SystemState::Fault_Mode != currentState) {
            if (HW1::Check_Connection() == 1) { 
                currentState = SystemState::Offline_Mode; // Needs to be declared Online_Mode in SystemModes if used, defaulting to logic
            } else {
                currentState = SystemState::Offline_Mode;
                BH5::alarm("CONNECTION_LOST");
            }
        }
        return 0;
    }
}