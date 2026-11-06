
#include "dashboard_manager.hpp"

#include <iostream>

// Deliver a message to the requested user's dashboard.
void ConsoleDashboardManager::deliver(
    const std::string& userId,
    const std::string& message
)
{
    std::cout << "\n================================\n";
    std::cout << "Dashboard for user: " << userId << "\n";
    std::cout << "================================\n";

    std::cout << message << "\n";

    std::cout << "================================\n";
}
