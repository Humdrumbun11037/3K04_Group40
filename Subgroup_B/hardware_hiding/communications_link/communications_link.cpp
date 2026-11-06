
#include "communications_link.hpp"
#include "HW1_Transceiver.hpp"

#include <string>

std::optional<std::vector<uint8_t>> HardwareCommsLink::receive()
{
    // Receive a message from the physical transceiver.
    std::string receivedMessage = HW1::Receive_Message();

    // An empty message means there is no packet available.
    if (receivedMessage.empty()) {
        return std::nullopt;
    }

    // Convert the received string into the byte format
    // expected by the HMS MessageRouter.
    std::vector<uint8_t> packet(
        receivedMessage.begin(),
        receivedMessage.end()
    );

    return packet;
}
