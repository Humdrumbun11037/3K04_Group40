#include "BH4_EventLogger.hpp"
#include "SW2_EventDatabase.hpp"

namespace BH4 {
    int generate_Event(std::string event) {
        std::string timestamped_event = "[TIMESTAMP] " + event;
        return SW2::add_event(timestamped_event);
    }
    
    std::string getEvents(int lowertimebound, int uppertimebound, std::string type) {
        return SW2::get_event(type); 
    }
}