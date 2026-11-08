//console input

/*
Hides: how a keyboard line meant for the integration (the demo "Swap ADC"
       choice) is taken before Subgroup A's main() reads it, without changing
       main(): a stream buffer put in front of std::cin, which reads one line
       at a time and offers it to a hook first

Interface
takes: keyboard lines
sends: every line the hook doesn't take on to Subgroup A's code, unchanged;
       lines the hook asks for itself (readLine) straight from the keyboard
uses: nothing
*/
#pragma once

#include <functional>
#include <optional>
#include <streambuf>
#include <string>

class ConsoleInput : public std::streambuf {
public:
    // Returns true if it took the line (Subgroup A's code never sees it).
    using LineHook = std::function<bool(const std::string& line)>;

    ConsoleInput(std::streambuf* keyboard, LineHook hook);

    // One line straight from the keyboard, without its newline; nothing at end of input.
    std::optional<std::string> readLine();

protected:
    int underflow() override;

private:
    std::streambuf* keyboard;
    LineHook hook;
    std::string pending;
};
