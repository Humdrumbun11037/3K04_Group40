// Tests for the demo cabinet selection (integration/adc_side/cabinet.*,
// console_input.*): the HMS's answers, the selection screen, loading a cabinet's
// stock into Subgroup A's real stock database (SW1), taking the "4" typed at the
// login menu, and the Subgroup A menu lines the screen hangs off. Run from integration/.

#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "BH3_InventoryManager.hpp"
#include "SW1_InventoryDatabase.hpp"
#include "SystemModes.hpp"
#include "adc_side/cabinet.hpp"
#include "adc_side/console_input.hpp"

// Subgroup A's main.cpp defines this; main.cpp is not linked into the test.
SystemState currentState = SystemState::Idle;

static int passed = 0;

static bool has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

static void parses_the_cabinet_list() {
    std::vector<cabinet::Cabinet> list = cabinet::parseList("1|Ward 3 West|simulated\n3|Ward 5|real\nbad line\n5|Emergency|simulated\n");
    assert(list.size() == 3);
    assert(list[0].id == 1 && list[0].location == "Ward 3 West" && !list[0].real);
    assert(list[1].id == 3 && list[1].real && list[2].location == "Emergency");
    assert(cabinet::parseList("").empty());
    passed++;
}

static void parses_a_claim_answer() {
    std::string error;
    std::optional<cabinet::Claimed> c = cabinet::parseClaim("OK|ICU|Morphine=60;Insulin=0", &error);
    assert(c && c->location == "ICU" && c->stock.size() == 2 && c->stock[0].first == "Morphine" && c->stock[1].second == 0);
    c = cabinet::parseClaim("OK|Emergency|", &error);
    assert(c && c->stock.empty());
    assert(!cabinet::parseClaim("ERR|ADC-9 is not registered on the HMS", &error) && error == "ADC-9 is not registered on the HMS");
    assert(!cabinet::parseClaim("garbage", &error));
    passed++;
}

static int pick(const std::vector<cabinet::Cabinet>& list, int current, const std::string& typed, std::string* shown = nullptr) {
    std::ostringstream out;
    bool asked = false;
    int id = cabinet::choose(list, current, out, [&]() -> std::optional<std::string> {
        if (asked || typed == "<eof>") return std::nullopt;
        asked = true;
        return typed;
    });
    if (shown) *shown = out.str();
    return id;
}

static void selection_screen() {
    std::vector<cabinet::Cabinet> list = {{1, "Ward 3 West", false}, {3, "Ward 5", false}, {4, "ER", true}};
    std::string shown;
    assert(pick(list, 3, "1", &shown) == 1);
    assert(has(shown, "SELECT ADC  (demo only: a real cabinet") && has(shown, " [1] ADC-1  Ward 3 West\n"));
    assert(has(shown, " [3] ADC-3  Ward 5  (this screen now)") && has(shown, " [4] ADC-4  ER  (another cabinet window has it)"));
    assert(pick(list, 0, " ADC-4 ") == 4);
    assert(pick(list, 0, "2") == 0);       // not listed
    assert(pick(list, 0, "Ward 5") == 0);
    assert(pick(list, 0, "<eof>") == 0);
    passed++;
}

// Subgroup A starts with Tylenol 100 (drawer 1) and Morphine 50 (drawer 2).
static void loads_a_cabinets_stock_into_subgroup_a() {
    cabinet::loadStock({{"Morphine", 40}, {"Insulin", 30}});   // ADC-1 on the HMS
    assert(SW1::get_drug_amount("Morphine") == 40 && SW1::get_drug_location("Morphine") == "2");
    assert(SW1::get_drug_amount("Insulin") == 30 && SW1::get_drug_location("Insulin") == "3");   // next free drawer
    assert(SW1::get_drug_amount("Tylenol") == 0);              // not stocked in this cabinet
    currentState = SystemState::Dispensation_Mode;
    assert(BH3::RequestDrug("Tylenol", 5, "mg", "nurse_jane") == -2);   // insufficient stock
    assert(BH3::RequestDrug("Insulin", 5, "mg", "nurse_jane") == 0);
    cabinet::loadStock({{"Tylenol", 100}, {"Morphine", 50}});   // swap back to ADC-3
    assert(SW1::get_drug_amount("Tylenol") == 100 && SW1::get_drug_amount("Morphine") == 50);
    assert(SW1::get_drug_amount("Insulin") == 0 && SW1::get_drug_location("Insulin") == "3");
    cabinet::loadStock({});   // a cabinet with nothing in it
    assert(SW1::get_drug_amount("Tylenol") == 0 && SW1::get_drug_amount("Morphine") == 0);
    assert(cabinet::describe({}) == "nothing stocked" && cabinet::describe({{"A", 1}, {"B", 2}}) == "A 1, B 2");
    passed++;
}

static void the_swap_key_is_taken_before_subgroup_a_reads_it() {
    std::istringstream keys("1\n4\n3\nx\r\n");
    std::vector<std::string> askedFor;
    ConsoleInput* self = nullptr;
    ConsoleInput input(keys.rdbuf(), [&](const std::string& line) {
        if (line != "4") return false;
        askedFor.push_back(*self->readLine());   // the hook asks its own question
        return true;
    });
    self = &input;
    std::istream in(&input);
    std::string a, b;
    in >> a >> b;
    assert(a == "1" && b == "x");          // Subgroup A's code never saw "4" or the "3" typed for the hook
    assert(askedFor.size() == 1 && askedFor[0] == "3");
    std::string rest;
    assert(!(in >> rest));                 // end of input passes through
    passed++;
}

static std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    assert(in && "Subgroup A source not found; run from integration/");
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

// The screen hangs off these lines of Subgroup A's main.cpp; if they change, the
// cabinet line and the Swap ADC option would silently disappear.
static void pinned_menu_lines_are_still_in_subgroup_a() {
    const std::string main = readFile("../Subgroup_A/src/main.cpp");
    assert(has(main, std::string("std::cout << \"") + cabinet::HEADER_LINE + "\\n\";"));
    assert(has(main, std::string("std::cout << \"") + cabinet::LOGIN_MENU_LAST + "\\n\";"));
    assert(has(main, "Please type a number 1-3."));   // Subgroup A has no option 4 of its own
    passed++;
}

int main() {
    parses_the_cabinet_list();
    parses_a_claim_answer();
    selection_screen();
    loads_a_cabinets_stock_into_subgroup_a();
    the_swap_key_is_taken_before_subgroup_a_reads_it();
    pinned_menu_lines_are_still_in_subgroup_a();
    std::cout << "cabinet_test: " << passed << " tests passed\n";
    return 0;
}
