#ifndef BH3_INVENTORY_MANAGER_HPP
#define BH3_INVENTORY_MANAGER_HPP

#include <string>

namespace BH3 {
    int RequestDrug(std::string drugname, int dose, std::string unit, std::string username);
    int addDrug(std::string drugname, int amount, std::string unit, int drawerID, std::string username);
    std::string getAvailableMedicines();
}

#endif