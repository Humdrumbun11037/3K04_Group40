#ifndef HW0_DRAWER_CONTROLLER_HPP
#define HW0_DRAWER_CONTROLLER_HPP

#include <string>

namespace HW0 {
    int Open_Drawer(int drawer_id);
    int Close_Drawer(int drawer_id);
    
    // Required by the Medicine Dispensing and Drawer Safety pseudocode
    int count_open_drawers();
    std::string check_drawer_status(int drawer_id);
}

#endif