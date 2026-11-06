
#include "clock.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

std::string SystemClock::now() const
{
    // Get the current system time.
    auto currentTime = std::chrono::system_clock::now();

    // Convert it to local time.
    std::time_t timeValue = std::chrono::system_clock::to_time_t(currentTime);
    std::tm* localTime = std::localtime(&timeValue);

    // Format the time as YYYY-MM-DD HH:MM:SS.
    std::ostringstream formattedTime;
    formattedTime << std::put_time(localTime, "%Y-%m-%d %H:%M:%S");

    return formattedTime.str();
}
