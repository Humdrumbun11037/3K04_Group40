#include "SW1_InventoryDatabase.hpp"
#include <unordered_map>

namespace SW1 {
    // Hidden Secret: Inventory data structure
    struct DrugInfo {
        int amount;
        int drawer_id;
    };
    
    static std::unordered_map<std::string, DrugInfo> inventory = {
        {"Tylenol", {100, 1}},
        {"Morphine", {50, 2}}
    };

    std::string get_drug_location(std::string drugname) {
        if (inventory.find(drugname) != inventory.end()) {
            return std::to_string(inventory[drugname].drawer_id);
        }
        return "-1";
    }

    int get_drug_amount(std::string drugname) {
        if (inventory.find(drugname) != inventory.end()) {
            return inventory[drugname].amount;
        }
        return 0;
    }

    int add_drug(std::string drugname, int drug_amount, int drawer_id) {
        inventory[drugname].amount += drug_amount;
        inventory[drugname].drawer_id = drawer_id;
        return 1;
    }

    int remove_drug(std::string drugname, int drug_amount, int drawer_id) {
        if (inventory.find(drugname) != inventory.end()) {
            inventory[drugname].amount -= drug_amount;
            return 1;
        }
        return 0;
    }
    std::string get_all_inventory() {
        std::string result = "\n--- CURRENT INVENTORY ---\n";
        for (const auto& item : inventory) {
            result += item.first + ": " + std::to_string(item.second.amount) + " mg\n";
        }
        result += "---------------------------\n";
        return result;
    }
}