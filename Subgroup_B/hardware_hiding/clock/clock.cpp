#include "clock.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

std::string SystemClock::now() const
{
    // Get the current time.
    auto currentTime = std::chrono::system_clock::now();

    // Convert it into a time value that can be formatted.
    std::time_t timeValue =
        std::chrono::system_clock::to_time_t(currentTime);

    // Convert into local calendar time.
    std::tm localTime{};

#ifdef _WIN32
    if (localtime_s(&localTime, &timeValue) != 0) {
        throw std::runtime_error("Unable to retrieve system time.");
    }
#else
    if (localtime_r(&timeValue, &localTime) == nullptr) {
        throw std::runtime_error("Unable to retrieve system time.");
    }
#endif

    // Format the time as YYYY-MM-DD HH:MM:SS.
    std::ostringstream formattedTime;

    formattedTime << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");

    return formattedTime.str();
}