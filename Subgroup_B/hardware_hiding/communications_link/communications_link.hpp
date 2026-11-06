
#ifndef COMMUNICATIONS_LINK_HPP
#define COMMUNICATIONS_LINK_HPP

#include "external_interfaces.hpp"

#include <cstdint>
#include <optional>
#include <vector>

class HardwareCommsLink : public CommsLink {
public:
    std::optional<std::vector<uint8_t>> receive() override;
};

#endif
