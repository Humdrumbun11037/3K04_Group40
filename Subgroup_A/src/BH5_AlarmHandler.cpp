#include "BH5_AlarmHandler.hpp"
#include "HW3_AlarmExecutor.hpp"
#include "HW0_DrawerController.hpp"
#include "SystemModes.hpp"

namespace BH5 {
    int alarm(std::string alarm_type) {
        if (alarm_type == "TIMEOUT_FAULT") {
            HW3::start_beep();
            HW3::start_strobe();
            HW0::Close_Drawer(-1);
            currentState = SystemState::Fault_Mode;
        } 
        else if (alarm_type == "SECURITY_BREACH") {
            HW3::start_beep();
            HW3::start_strobe();
            HW3::alert_security();
            HW0::Close_Drawer(-1);
            currentState = SystemState::Security_Lockout;
        }
        else if (alarm_type == "INVALID_LOGIN_MAX") {
            HW3::start_beep();
            HW3::start_strobe();
            HW3::alert_security();
            currentState = SystemState::Security_Lockout;
        }
        else if (alarm_type == "LOW_INVENTORY") {
            HW3::start_strobe(); 
        }
        else if (alarm_type == "HARDWARE_JAM") {
            HW3::start_beep();
            HW0::Close_Drawer(-1);
            currentState = SystemState::Fault_Mode;
        }
        else if (alarm_type == "CONNECTION_LOST") {
            HW3::start_beep();
            HW3::start_strobe();
            currentState = SystemState::Offline_Mode;
        }
        return 0;
    }

    void stop_alarm() {
        HW3::stop_beep();
        HW3::stop_strobe();
    }
}