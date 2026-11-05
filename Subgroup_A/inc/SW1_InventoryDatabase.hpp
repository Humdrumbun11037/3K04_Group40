#ifndef SW1_INVENTORY_DATABASE_HPP
#define SW1_INVENTORY_DATABASE_HPP

#include <string>

namespace SW1 {
    std::string get_drug_location(std::string drugname);
    int get_drug_amount(std::string drugname);
    int add_drug(std::string drugname, int drug_amount, int drawer_id);
    int remove_drug(std::string drugname, int drug_amount, int drawer_id);
    std::string get_all_inventory();
}

#endif