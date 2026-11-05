#include "SW2_EventDatabase.hpp"
#include <vector>
#include <iostream>

namespace SW2 {
    // Hidden Secret: Persistent log structure
    static std::vector<std::string> event_log;

    int add_event(std::string event_string) {
        event_log.push_back(event_string);
        // Echo to console just so you can see it working during testing
        std::cout << "[SW-2 EVENT LOG SAVED]: " << event_string << "\n";
        return 1;
    }

    std::string get_event(std::string event) {
        std::string result = "";
        for (const auto& e : event_log) {
            if (e.find(event) != std::string::npos) {
                result += e + "\n";
            }
        }
        return result.empty() ? "No matching events" : result;
    }
}