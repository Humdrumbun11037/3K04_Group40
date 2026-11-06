#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <limits>
#include <algorithm>
#include <cctype>

#include "SystemModes.hpp"
#include "BH0_HMSComms.hpp"
#include "BH2_LoginAuthenticator.hpp"
#include "BH3_InventoryManager.hpp"
#include "HW2_DisplayController.hpp"
#include "BH4_EventLogger.hpp"
#include "BH5_AlarmHandler.hpp"
#include "SW0_UserDatabase.hpp"

// Global state variables
SystemState currentState = SystemState::Idle;
std::string currentUserAuthority = ""; 
std::string currentUsername = ""; 

void FormatInput(std::string &str) {
    if(str.empty()) return;
    str[0] = std::toupper(str[0]);
    for(size_t i = 1; i < str.length(); ++i) {
        str[i] = std::tolower(str[i]);
    }
}

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
        BH3::RequestDrug("Tylenol", 10, "mg", "nurse_jane");
    }
    std::this_thread::sleep_for(std::chrono::seconds(1)); 
}

void TestSecurityLockout() {
    printTestHeader("2. EDGE CASE: SECURITY LOCKOUT");
    std::cout << "Attempt 1...\n";
    BH2::check_user("nurse_jane", "wrong_pass");
    std::cout << "Attempt 2...\n";
    BH2::check_user("nurse_jane", "wrong_pass");
    std::cout << "Attempt 3 (Lockout!)...\n";
    BH2::check_user("nurse_jane", "wrong_pass");
    
    BH2::reset_attempts(); 
    BH5::stop_alarm();
}

void TestLowInventory() {
    printTestHeader("3. EDGE CASE: LOW INVENTORY");
    currentState = SystemState::Dispensation_Mode;
    BH2::check_user("nurse_jane", "hash123");
    BH3::RequestDrug("Tylenol", 500, "mg", "nurse_jane");
}

void TestPharmacistRestock() {
    printTestHeader("4. NORMAL PATH: PHARMACIST RESTOCK");
    std::string authority = BH2::check_user("pharm_bob", "hash456");
    if (authority == "Pharmacist") {
        HW2::display_text("Pharmacist verified. Restocking Morphine.");
        BH3::addDrug("Morphine", 500, "mg", 2, "pharm_bob");
        std::cout << "[SUCCESS] Morphine stock updated in SW-1.\n";
    }
}

void TestNetworkLoss() {
    printTestHeader("5. EDGE CASE: NETWORK LOSS");
    std::cout << "Simulating HMS connection drop...\n";
    BH5::alarm("CONNECTION_LOST");
    std::cout << "[SYSTEM] State transitioned to OFFLINE_MODE.\n";
}

void TestHardwareJam() {
    printTestHeader("6. EDGE CASE: HARDWARE JAM");
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
    std::cout << "\n[TESTS COMPLETE] Returning to Login Screen...\n";
}

// ======================================================
// 2. INTERACTIVE VISUAL MENUS
// ======================================================
void PrintSystemHeader() {
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
}

void ShowLoginMenu() {
    PrintSystemHeader();
    std::cout << " [1] Secure User Login\n";
    std::cout << " [2] Run Automated Test Suite\n";
    std::cout << " [3] Shutdown System\n";
    std::cout << "========================================\n";
    std::cout << " Select an option: ";
}

void ShowAuthenticatedMenu() {
    PrintSystemHeader();
    std::cout << " Logged in as: " << currentUsername << " (" << currentUserAuthority << ")\n";
    std::cout << "----------------------------------------\n";
    std::cout << " [1] Request Medication (Nurse/Pharmacist)\n";
    std::cout << " [2] Restock Medication (Pharmacist Only)\n";
    std::cout << " [3] Simulate Hardware Jam during Dispense\n";
    std::cout << " [4] View Event Logs\n";
    std::cout << " [5] Ping HMS Network (Test Connection)\n";
    std::cout << " [6] Add System User (Admin Only)\n";
    std::cout << " [7] Remove System User (Admin Only)\n";
    std::cout << " [8] Clear Faults / Hardware Reset (Technician Only)\n";
    std::cout << " [9] Secure Logout\n";
    std::cout << " [10] Shutdown System\n";
    std::cout << "========================================\n";
    std::cout << " Select an option: ";
}

int main() {
    bool running = true;
    std::string input;
    
    while (running) {
        // --- UNAUTHENTICATED STATE ---
        if (currentUserAuthority.empty()) {
            ShowLoginMenu();
            std::cin >> input;
            
            if (input == "1") {
                std::string user, pass;
                std::cout << "\n--- SECURE LOGIN ---\n";
                std::cout << "Username: ";
                std::cin >> user;
                std::cout << "Password: ";
                std::cin >> pass;
                
                std::string auth = BH2::check_user(user, pass);
                if (auth == "Nurse" || auth == "Pharmacist" || auth == "Technician" || auth == "Admin") {
                    
                    if ((currentState == SystemState::Security_Lockout || currentState == SystemState::Fault_Mode) && auth != "Technician") {
                        std::cout << "\n[!] SYSTEM LOCKED. Only Technicians may log in at this time.\n";
                    } else {
                        std::cout << "\n[SUCCESS] Authentication verified. Welcome, " << auth << ".\n";
                        if (currentState == SystemState::Idle || currentState == SystemState::Offline_Mode) {
                            currentState = SystemState::Dispensation_Mode; 
                        }
                        currentUserAuthority = auth;
                        currentUsername = user; 
                    }
                    
                } else {
                    std::cout << "\n[FAILED] Invalid credentials.\n";
                    if (currentState == SystemState::Security_Lockout) {
                        std::cout << "\n[CRITICAL ALARM] MAXIMUM ATTEMPTS REACHED. SECURITY LOCKDOWN INITIATED.\n";
                    }
                }
            }
            else if (input == "2") {
                RunAllTests();
                BH5::stop_alarm();
                currentState = SystemState::Idle; 
                currentUserAuthority = ""; 
                currentUsername = "";
            }
            else if (input == "3") {
                std::cout << "\nShutting down ADC...\n";
                running = false;
            }
            else {
                std::cout << "\n[!] Invalid selection. Please type a number 1-3.\n";
            }
        } 
        
        // --- AUTHENTICATED STATE ---
        else {
            ShowAuthenticatedMenu();
            std::cin >> input;
            
            // SYSTEM LOCK GUARD: Blocks all standard interactions if system is faulted
            if ((currentState == SystemState::Fault_Mode || currentState == SystemState::Security_Lockout) 
                && input != "8" && input != "9" && input != "10") {
                std::cout << "\n[!] SYSTEM LOCKED. You must log out (Option 9) and have a Technician clear the fault.\n";
                continue;
            }
            
            if (input == "1") {
                if (currentUserAuthority != "Nurse" && currentUserAuthority != "Pharmacist") {
                    std::cout << "\n[!] ACCESS DENIED: Only Nurses and Pharmacists can dispense medication.\n";
                    continue;
                }
                
                std::string drug;
                std::string doseInput;
                int dose = 0;
                
                std::cout << "\n--- DISPENSE MEDICATION ---";
                std::cout << BH3::getAvailableMedicines(); 
                std::cout << "Drug Name: ";
                std::getline(std::cin >> std::ws, drug);
                FormatInput(drug); 
                
                std::cout << "Dose Amount (mg): ";
                while (true) {
                    std::cin >> doseInput;
                    bool isNumber = true;
                    for (char const &c : doseInput) {
                        if (std::isdigit(c) == 0) isNumber = false;
                    }
                    if (isNumber) {
                        try {
                            dose = std::stoi(doseInput);
                            if (dose > 0) break; 
                            else std::cout << "Amount must be greater than zero. Try again: ";
                        } catch (...) {
                            std::cout << "Number too large. Try again: ";
                        }
                    } else {
                        std::cout << "Invalid input. Only positive whole numbers allowed. Try again: ";
                    }
                }
                
                BH3::RequestDrug(drug, dose, "mg", currentUsername);
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
            else if (input == "2") {
                if (currentUserAuthority != "Pharmacist") {
                    std::cout << "\n[!] ACCESS DENIED: Only a logged-in Pharmacist can restock medication.\n";
                    continue;
                }
                
                std::string drug;
                std::string amountInput;
                int amount = 0;
                
                std::cout << "\n--- RESTOCK MEDICATION ---";
                std::cout << BH3::getAvailableMedicines(); 
                std::cout << "Drug Name: ";
                std::getline(std::cin >> std::ws, drug);
                FormatInput(drug); 
                
                std::cout << "Amount to Add: ";
                while (true) {
                    std::cin >> amountInput;
                    bool isNumber = true;
                    for (char const &c : amountInput) {
                        if (std::isdigit(c) == 0) isNumber = false;
                    }
                    if (isNumber) {
                        try {
                            amount = std::stoi(amountInput);
                            if (amount > 0) break; 
                            else std::cout << "Amount must be greater than zero. Try again: ";
                        } catch (...) {
                            std::cout << "Number too large. Try again: ";
                        }
                    } else {
                        std::cout << "Invalid input. Only positive whole numbers allowed. Try again: ";
                    }
                }
                
                BH3::addDrug(drug, amount, "mg", 1, currentUsername); 
                std::cout << "[SUCCESS] Inventory database updated for " << drug << ".\n";
            }
            // NEW: Simulate Hardware Jam during interaction
            else if (input == "3") {
                if (currentUserAuthority != "Nurse" && currentUserAuthority != "Pharmacist") {
                    std::cout << "\n[!] ACCESS DENIED: Only Nurses and Pharmacists can dispense medication.\n";
                    continue;
                }
                
                std::string drug;
                std::string doseInput;
                int dose = 0;
                
                std::cout << "\n--- [SIMULATION] HARDWARE JAM DURING DISPENSE ---\n";
                std::cout << BH3::getAvailableMedicines(); 
                std::cout << "Drug Name: ";
                std::getline(std::cin >> std::ws, drug);
                FormatInput(drug); 
                
                std::cout << "Dose Amount (mg): ";
                while (true) {
                    std::cin >> doseInput;
                    bool isNumber = true;
                    for (char const &c : doseInput) {
                        if (std::isdigit(c) == 0) isNumber = false;
                    }
                    if (isNumber) {
                        try {
                            dose = std::stoi(doseInput);
                            if (dose > 0) break; 
                            else std::cout << "Amount must be greater than zero. Try again: ";
                        } catch (...) {
                            std::cout << "Number too large. Try again: ";
                        }
                    } else {
                        std::cout << "Invalid input. Only positive whole numbers allowed. Try again: ";
                    }
                }
                
                std::cout << "\n[SYSTEM] Attempting to actuate drawer for " << drug << "...\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(800));
                
                BH5::alarm("HARDWARE_JAM");
                currentState = SystemState::Fault_Mode;
                
                std::cout << "\n[CRITICAL FAULT] Drawer actuator jammed! (Error Code 3)\n";
                std::cout << "[!] SYSTEM LOCKED. Please log out and have a Technician clear the fault.\n";
                
                BH4::generate_Event(currentUsername + " attempted to dispense " + std::to_string(dose) + "mg of " + drug + " but caused a HARDWARE JAM");
            }
            else if (input == "4") {
                std::cout << "\n--- SYSTEM EVENT LOGS ---\n";
                std::string logs = BH4::getEvents(0, 0, "");
                std::cout << logs << "\n";
            }
            else if (input == "5") {
                std::cout << "\n--- PINGING HMS ---\n";
                BH0::verify_connection();
                std::cout << "[SUCCESS] Ping complete. Hardware returned OK.\n";
            }
            else if (input == "6") {
                if (currentUserAuthority != "Admin") {
                    std::cout << "\n[!] ACCESS DENIED: Only an Admin can add users.\n";
                    continue;
                }
                
                std::string newUser, newPass, newRoleChoice, newRole;
                std::cout << "\n--- ADD SYSTEM USER ---\n";
                std::cout << "New Username: ";
                std::cin >> newUser;
                std::cout << "New Password: ";
                std::cin >> newPass;
                std::cout << "Role [1] Nurse [2] Pharmacist [3] Technician [4] Admin: ";
                std::cin >> newRoleChoice;
                
                if (newRoleChoice == "1") newRole = "Nurse";
                else if (newRoleChoice == "2") newRole = "Pharmacist";
                else if (newRoleChoice == "3") newRole = "Technician";
                else if (newRoleChoice == "4") newRole = "Admin";
                else {
                    std::cout << "[!] Invalid role selection.\n";
                    continue;
                }
                
                std::string res = SW0::add_user(newUser, newPass, newRole);
                if (res == "Success") {
                    BH4::generate_Event(currentUsername + " added new system user: " + newUser + " (" + newRole + ")");
                    std::cout << "[SUCCESS] User '" << newUser << "' added successfully.\n";
                } else {
                    std::cout << "[!] User already exists in database.\n";
                }
            }
            else if (input == "7") {
                if (currentUserAuthority != "Admin") {
                    std::cout << "\n[!] ACCESS DENIED: Only an Admin can remove users.\n";
                    continue;
                }
                
                std::string targetUser;
                std::cout << "\n--- REMOVE SYSTEM USER ---\n";
                std::cout << "Username to remove: ";
                std::cin >> targetUser;
                
                std::string targetRole = SW0::get_user_role(targetUser);
                if (targetRole == "Invalid") {
                    std::cout << "[!] User not found in database.\n";
                    continue;
                }
                
                if (targetUser == currentUsername) {
                    std::cout << "[!] Cannot remove yourself while currently logged in.\n";
                    continue;
                }
                
                if (targetRole == "Nurse" && SW0::count_role("Nurse") <= 1) {
                    std::cout << "[!] ACTION DENIED: System must have at least 1 Nurse.\n";
                    continue;
                }
                if (targetRole == "Pharmacist" && SW0::count_role("Pharmacist") <= 1) {
                    std::cout << "[!] ACTION DENIED: System must have at least 1 Pharmacist.\n";
                    continue;
                }
                if (targetRole == "Admin" && SW0::count_role("Admin") <= 1) {
                    std::cout << "[!] ACTION DENIED: System must have at least 1 Admin.\n";
                    continue;
                }
                
                SW0::remove_user(targetUser);
                BH4::generate_Event(currentUsername + " removed system user: " + targetUser);
                std::cout << "[SUCCESS] User '" << targetUser << "' removed.\n";
            }
            else if (input == "8") {
                if (currentUserAuthority != "Technician") {
                    std::cout << "\n[!] ACCESS DENIED: Only a Technician can clear hardware faults.\n";
                    continue;
                }
                
                std::cout << "\n[SYSTEM RESET] Technician cleared faults. Returning to IDLE state.\n";
                BH5::stop_alarm();
                BH2::reset_attempts();
                currentState = SystemState::Idle;
            }
            else if (input == "9") {
                std::cout << "\n[LOGOUT] Securely terminating session. Returning to Idle...\n";
                currentUserAuthority = "";
                currentUsername = "";
                if (currentState != SystemState::Security_Lockout && currentState != SystemState::Fault_Mode) {
                    currentState = SystemState::Idle;
                }
            }
            else if (input == "10") {
                std::cout << "\nShutting down ADC...\n";
                running = false;
            }
            else {
                std::cout << "\n[!] Invalid selection. Please type a number 1-10.\n";
            }
        }
    }
    
    return 0;
}