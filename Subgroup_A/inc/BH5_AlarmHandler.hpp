#ifndef BH5_ALARM_HANDLER_HPP
#define BH5_ALARM_HANDLER_HPP

#include <string>

namespace BH5 {
    int alarm(std::string alarm_type);
    void stop_alarm();
}

#endif