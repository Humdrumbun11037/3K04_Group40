#include <iostream>
#include <thread>
#include <chrono>

#include "SystemModes.hpp"
#include "BH2_LoginAuthenticator.hpp"
#include "BH3_InventoryManager.hpp"
#include "HW2_DisplayController.hpp"

// 1. Define the global state variable here
SystemState currentState = SystemState::Idle;

int main() {
    std::cout << "Starting ADC System...\n";
    
    // 2. Simulate setting the system to dispense mode
    currentState = SystemState::Dispensation_Mode;
    
    // 3. Simulate a user walking up and dispensing medicine
    std::string authority = BH2::check_user("nurse_jane", "hash123");
    
    if (authority == "Nurse" || authority == "Pharmacist") {
        HW2::display_text("Login valid. Proceeding to dispense.");
        
        // This will trigger HW-0, SW-1, BH-4, and the 30-second Watchdog thread
        BH3::RequestDrug("Tylenol", 10, "mg");
    } else {
        HW2::display_text("Login failed.");
    }

    // 4. Keep the main thread alive briefly so the concurrent safety thread can print its output
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    std::cout << "\nADC System shutting down...\n";
    return 0;
}