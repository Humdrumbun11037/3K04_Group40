#ifndef SW2_EVENT_DATABASE_HPP
#define SW2_EVENT_DATABASE_HPP

#include <string>

namespace SW2 {
    int add_event(std::string event_string);
    std::string get_event(std::string event);
}

#endif