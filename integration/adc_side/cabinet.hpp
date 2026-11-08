//cabinet selection (demo only)

/*
Hides: which cabinet this ADC program is (demo only: a real cabinet is one ADC
       with its own screen and never changes), the selection screen, the HMS's
       cabinet list and stock answers (ADC_HMS_INTERFACE.md section 7), how a
       cabinet's stock is loaded into Subgroup A's stock database (SW1, through
       its own public functions), and the lines of Subgroup A's menu the screen
       hangs off

Interface
takes: the HMS's answers; the user's typed choice
sends: the chosen ADC id; Subgroup A's stock set to that cabinet's counts
uses: Subgroup A's SW1_InventoryDatabase (unchanged)
*/
#pragma once

#include <functional>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace cabinet {

// Lines Subgroup A's main.cpp prints (ShowLoginMenu, PrintSystemHeader). The
// integration adds its own lines after them; tests/cabinet_test pins them.
constexpr const char* HEADER_LINE = "   AUTOMATED DISPENSING CABINET (ADC)   ";
constexpr const char* LOGIN_MENU_LAST = " [3] Shutdown System";
constexpr const char* SWAP_OPTION = " [4] Swap ADC (demo only)";
constexpr const char* SWAP_KEY = "4";

struct Cabinet {
    int id;
    std::string location;
    bool real;   // another cabinet program drives it right now
};

using Stock = std::vector<std::pair<std::string, int>>;

struct Claimed {
    std::string location;
    Stock stock;
};

// "<id>|<location>|real|simulated" lines.
std::vector<Cabinet> parseList(const std::string& text);
// "OK|<location>|<med>=<n>;..." or "ERR|<reason>" (reason into *error).
std::optional<Claimed> parseClaim(const std::string& answer, std::string* error);

// The boxed selection screen. Returns the chosen ADC id, or 0 if the choice
// isn't one of the listed ADCs (or input ended).
int choose(const std::vector<Cabinet>& list, int current, std::ostream& out,
           const std::function<std::optional<std::string>()>& readLine);

// Sets Subgroup A's stock to exactly these counts. A medication it holds that
// the cabinet doesn't is set to 0; a new one gets the next free drawer.
void loadStock(const Stock& stock);

std::string describe(const Stock& stock);   // "Tylenol 100, Morphine 50"

}  // namespace cabinet
