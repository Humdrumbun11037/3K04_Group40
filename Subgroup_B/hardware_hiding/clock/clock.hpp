#ifndef CLOCK_HPP
#define CLOCK_HPP

#include "external_interfaces.hpp"

#include <string>

// Hardware Hiding implementation of the Software Decisions Clock interface.
class SystemClock : public Clock {
public:
    std::string now() const override;
};

#endif
