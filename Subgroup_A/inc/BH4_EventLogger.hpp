#ifndef BH4_EVENT_LOGGER_HPP
#define BH4_EVENT_LOGGER_HPP

#include <string>

namespace BH4 {
    int generate_Event(std::string event);
    std::string getEvents(int lowertimebound, int uppertimebound, std::string type);
}

#endif