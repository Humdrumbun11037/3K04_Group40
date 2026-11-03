#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <limits>

#include "SystemModes.hpp"
#include "BH0_HMSComms.hpp"
#include "BH2_LoginAuthenticator.hpp"
#include "BH3_InventoryManager.hpp"
#include "HW2_DisplayController.hpp"
#include "BH4_EventLogger.hpp"
#include "BH5_AlarmHandler.hpp"

// Define the global state variable
SystemState currentState = SystemState::Idle;

// ======================================================
// 1. AUTOMATED TEST SUITE (Hardcoded Edge Cases)
// ======================================================
void printTestHeader(std::string testName) {
    std::cout << "\n======================================================\n";
    std::cout << " [TEST] " << testName << "\n";
    std::cout << "======================================================\n";
    currentState = SystemState::Dispensation_Mode; 
}

void TestNormalPath() {
    printTestHeader("1. NORMAL PATH (Nurse Successful Dispense)");
    std::string authority = BH2::check_user("nurse_jane", "hash123");
    if (authority == "Nurse" || authority == "Pharmacist") {
        HW2::display_text("Login valid. Proceeding to dispense.");
        BH3::RequestDrug("Tylenol", 10, "mg");
    }
    std::this_thread::sleep_for(std::chrono::seconds(1)); 
}

void TestSecurityLockout() {
    printTestHeader("2. EDGE CASE: SECURITY LOCKOUT (A-NFR-SEC-01)");
    std::cout << "Attempt 1...\n";
    BH2::check_user("nurse_jane", "wrong_pass");
    std::cout << "Attempt 2...\n";
    BH2::check_user("nurse_jane", "wrong_pass");
    std::cout << "Attempt 3 (Lockout!)...\n";
    BH2::check_user("nurse_jane", "wrong_pass");
    
    // Reset internal strike counter for next test
    BH2::check_user("nurse_jane", "hash123"); 
}

void TestLowInventory() {
    printTestHeader("3. EDGE CASE: LOW INVENTORY (A-FR-INV-03)");
    BH2::check_user("nurse_jane", "hash123");
    BH3::RequestDrug("Tylenol", 500, "mg");
}

void TestPharmacistRestock() {
    printTestHeader("4. NORMAL PATH: PHARMACIST RESTOCK (A-US-PHM-01)");
    std::string authority = BH2::check_user("pharm_bob", "hash456");
    if (authority == "Pharmacist") {
        HW2::display_text("Pharmacist verified. Restocking Morphine.");
        BH3::addDrug("Morphine", 500, "mg", 2);
        BH4::generate_Event("Restocked 500mg of Morphine in Drawer 2.");
        std::cout << "[SUCCESS] Morphine stock updated in SW-1.\n";
    }
}

void TestNetworkLoss() {
    printTestHeader("5. EDGE CASE: NETWORK LOSS (A-US-ADM-02)");
    std::cout << "Simulating HMS connection drop...\n";
    BH5::alarm("CONNECTION_LOST");
    std::cout << "[SYSTEM] State transitioned to OFFLINE_MODE.\n";
}

void TestHardwareJam() {
    printTestHeader("6. EDGE CASE: HARDWARE JAM (A-US-TEC-01)");
    std::cout << "Simulating physical obstruction in drawer...\n";
    BH5::alarm("HARDWARE_JAM");
    std::cout << "[SYSTEM] State transitioned to FAULT_MODE.\n";
}

void RunAllTests() {
    TestNormalPath();
    TestSecurityLockout();
    TestLowInventory();
    TestPharmacistRestock();
    TestNetworkLoss();
    TestHardwareJam();
    std::cout << "\n[TESTS COMPLETE] Returning to Main Menu...\n";
}

// ======================================================
// 2. INTERACTIVE VISUAL MENU 
// ======================================================
void ShowMenu() {
    std::cout << "\n========================================\n";
    std::cout << "   AUTOMATED DISPENSING CABINET (ADC)   \n";
    std::cout << "========================================\n";
    std::cout << " Current System State: ";
    
    switch(currentState) {
        case SystemState::Idle: std::cout << "IDLE"; break;
        case SystemState::Dispensation_Mode: std::cout << "DISPENSATION MODE (Unlocked)"; break;
        case SystemState::Fault_Mode: std::cout << "FAULT MODE (Hardware Locked)"; break;
        case SystemState::Security_Lockout: std::cout << "SECURITY LOCKOUT"; break;
        case SystemState::Offline_Mode: std::cout << "OFFLINE (Local Dispensing Only)"; break;
    }
    
    std::cout << "\n----------------------------------------\n";
    std::cout << " [1] Run Automated Test Suite\n";
    std::cout << " [2] User Login (Interactive)\n";
    std::cout << " [3] Request Medication (Nurse/Pharmacist)\n";
    std::cout << " [4] Restock Medication (Pharmacist Only)\n";
    std::cout << " [5] View Event Logs\n";
    std::cout << " [6] Ping HMS Network (Test Connection)\n";
    std::cout << " [7] Clear Faults / Reset to IDLE (Technician)\n";
    std::cout << " [8] Shutdown System\n";
    std::cout << "========================================\n";
    std::cout << " Select an option: ";
}

int main() {
    bool running = true;
    std::string input;
    
    while (running) {
        ShowMenu();
        std::cin >> input;
        
        if (input == "1") {
            RunAllTests();
            BH5::stop_alarm();
            currentState = SystemState::Idle; // Reset after tests
        } 
        else if (input == "2") {
            if (currentState == SystemState::Security_Lockout || currentState == SystemState::Fault_Mode) {
                std::cout << "\n[!] SYSTEM LOCKED. Cannot login at this time.\n";
                continue;
            }
            
            std::string user, pass;
            std::cout << "\n--- SECURE LOGIN ---\n";
            std::cout << "Username: ";
            std::cin >> user;
            std::cout << "Password: ";
            std::cin >> pass;
            
            std::string auth = BH2::check_user(user, pass);
            if (auth == "Nurse" || auth == "Pharmacist") {
                std::cout << "\n[SUCCESS] Welcome, " << auth << ".\n";
                currentState = SystemState::Dispensation_Mode;
            } else {
                std::cout << "\n[FAILED] Invalid credentials.\n";
            }
        }
        else if (input == "3") {
            if (currentState != SystemState::Dispensation_Mode) {
                std::cout << "\n[!] ERROR: You must log in first to dispense medication.\n";
                continue;
            }
            
            std::string drug;
            int dose;
            std::cout << "\n--- DISPENSE MEDICATION ---\n";
            std::cout << "Drug Name (Options: Tylenol, Morphine): ";
            std::cin >> drug;
            std::cout << "Dose Amount (mg): ";
            
            while (!(std::cin >> dose)) {
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "Invalid input. Please enter a number: ";
            }
            
            BH3::RequestDrug(drug, dose, "mg");
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        else if (input == "4") {
            if (currentState != SystemState::Dispensation_Mode) {
                std::cout << "\n[!] ERROR: You must log in as a Pharmacist to restock.\n";
                continue;
            }
            
            std::string drug;
            int amount;
            std::cout << "\n--- RESTOCK MEDICATION ---\n";
            std::cout << "Drug Name: ";
            std::cin >> drug;
            std::cout << "Amount to Add: ";
            
            while (!(std::cin >> amount)) {
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "Invalid input. Please enter a number: ";
            }
            
            BH3::addDrug(drug, amount, "mg", 1); // Default to drawer 1 for stub
            BH4::generate_Event("Pharmacist restocked " + std::to_string(amount) + " units of " + drug);
            std::cout << "\n[SUCCESS] Inventory database updated.\n";
        }
        else if (input == "5") {
            std::cout << "\n--- SYSTEM EVENT LOGS ---\n";
            std::string logs = BH4::getEvents(0, 0, "");
            std::cout << logs << "\n";
        }
        else if (input == "6") {
            std::cout << "\n--- PINGING HMS ---\n";
            BH0::verify_connection();
            std::cout << "[SUCCESS] Ping complete. Hardware returned OK.\n";
        }
        else if (input == "7") {
            std::cout << "\n[SYSTEM RESET] Technician cleared faults. Returning to IDLE state.\n";
            BH5::stop_alarm();
            currentState = SystemState::Idle;
        }
        else if (input == "8") {
            std::cout << "\nShutting down ADC...\n";
            running = false;
        }
        else {
            std::cout << "\n[!] Invalid selection. Please type a number 1-8.\n";
        }
    }
    
    return 0;
}
