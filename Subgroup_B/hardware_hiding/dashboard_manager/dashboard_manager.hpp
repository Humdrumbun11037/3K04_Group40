
#ifndef DASHBOARD_MANAGER_HPP
#define DASHBOARD_MANAGER_HPP

#include "external_interfaces.hpp"

#include <string>

// Hardware Hiding implementation of the dashboard output interface.
class ConsoleDashboardManager : public DashboardOutput {
public:
    // Deliver a message to a specific user's dashboard.
    void deliver(
        const std::string& userId,
        const std::string& message
    ) override;
};

#endif
