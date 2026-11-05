#include "HW2_DisplayController.hpp"
#include <iostream>

namespace HW2 {
    std::string display_text(std::string text) {
        // Outputs the provided string to the physical screen[cite: 10]
        std::cout << "\n========================================\n";
        std::cout << "[DISPLAY]: " << text << "\n";
        std::cout << "========================================\n";
        return "Success";
    }
    
    std::string get_input() {
        // Halts execution to wait for a physical keyboard press[cite: 10]
        std::string input;
        std::cout << "[KEYBOARD INPUT] > ";
        std::cin >> input;
        return input;
    }
    
    int clear_display() {
        // Flushes the screen buffer[cite: 10]
        std::cout << "\n\n\n\n\n"; 
        return 1;
    }
}