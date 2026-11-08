//cabinet selection (demo only)

#include "cabinet.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

#include "SW1_InventoryDatabase.hpp"

namespace cabinet {

namespace {

std::vector<std::string> split(const std::string& text, char by) {
    std::vector<std::string> parts;
    std::string part;
    std::istringstream in(text);
    while (std::getline(in, part, by)) parts.push_back(part);
    if (!text.empty() && text.back() == by) parts.push_back("");
    return parts;
}

bool wholeNumber(const std::string& s) {
    return !s.empty() && s.size() <= 9
           && std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
}

std::string trim(const std::string& s) {
    std::size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

// The medication names Subgroup A's stock holds, from its own listing
// ("Name: N mg" lines, SW1::get_all_inventory).
std::vector<std::string> heldMedications() {
    std::vector<std::string> names;
    std::istringstream in(SW1::get_all_inventory());
    for (std::string line; std::getline(in, line);) {
        std::size_t colon = line.rfind(": ");
        if (colon != std::string::npos && line.size() > 3 && line.compare(line.size() - 3, 3, " mg") == 0) {
            names.push_back(line.substr(0, colon));
        }
    }
    return names;
}

void setCount(const std::string& name, int target, int& nextDrawer) {
    std::string location = SW1::get_drug_location(name);
    int drawer = location == "-1" ? nextDrawer++ : std::stoi(location);
    int current = location == "-1" ? 0 : SW1::get_drug_amount(name);
    if (location == "-1" || target > current) SW1::add_drug(name, target - current, drawer);
    else if (target < current) SW1::remove_drug(name, current - target, drawer);
}

}  // namespace

std::vector<Cabinet> parseList(const std::string& text) {
    std::vector<Cabinet> list;
    std::istringstream in(text);
    for (std::string line; std::getline(in, line);) {
        std::vector<std::string> f = split(trim(line), '|');
        if (f.size() == 3 && wholeNumber(f[0])) list.push_back({std::stoi(f[0]), f[1], f[2] == "real"});
    }
    return list;
}

std::optional<Claimed> parseClaim(const std::string& answer, std::string* error) {
    std::vector<std::string> f = split(answer, '|');
    if (f.size() == 3 && f[0] == "OK") {
        Claimed c{f[1], {}};
        for (const std::string& item : split(f[2], ';')) {
            std::size_t eq = item.rfind('=');
            if (eq != std::string::npos && wholeNumber(item.substr(eq + 1))) {
                c.stock.push_back({item.substr(0, eq), std::stoi(item.substr(eq + 1))});
            }
        }
        return c;
    }
    if (error) *error = f.size() >= 2 && f[0] == "ERR" ? f[1] : "unexpected answer from the HMS";
    return std::nullopt;
}

int choose(const std::vector<Cabinet>& list, int current, std::ostream& out,
           const std::function<std::optional<std::string>()>& readLine) {
    out << "\n========================================\n"
        << "  SELECT ADC  (demo only: a real cabinet\n"
        << "  is one ADC with its own screen)\n"
        << "========================================\n";
    for (const Cabinet& c : list) {
        out << " [" << c.id << "] ADC-" << c.id << "  " << c.location;
        if (c.id == current) out << "  (this screen now)";
        else if (c.real) out << "  (another cabinet window has it)";
        out << "\n";
    }
    out << " Select: " << std::flush;
    std::optional<std::string> typed = readLine();
    if (!typed) return 0;
    std::string choice = trim(*typed);
    if (choice.rfind("ADC-", 0) == 0) choice = choice.substr(4);
    if (!wholeNumber(choice)) return 0;
    int id = std::stoi(choice);
    for (const Cabinet& c : list) {
        if (c.id == id) return id;
    }
    return 0;
}

void loadStock(const Stock& stock) {
    int nextDrawer = 1;
    std::vector<std::string> held = heldMedications();
    for (const std::string& name : held) nextDrawer = std::max(nextDrawer, std::stoi(SW1::get_drug_location(name)) + 1);
    for (const std::string& name : held) {
        bool kept = std::any_of(stock.begin(), stock.end(), [&](const auto& s) { return s.first == name; });
        if (!kept) setCount(name, 0, nextDrawer);
    }
    for (const auto& [name, count] : stock) setCount(name, count, nextDrawer);
}

std::string describe(const Stock& stock) {
    std::string out;
    for (const auto& [name, count] : stock) out += (out.empty() ? "" : ", ") + name + " " + std::to_string(count);
    return out.empty() ? "nothing stocked" : out;
}

}  // namespace cabinet
