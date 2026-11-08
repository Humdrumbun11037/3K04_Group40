//console input

#include "console_input.hpp"

ConsoleInput::ConsoleInput(std::streambuf* keyboard, LineHook hook)
    : keyboard(keyboard), hook(std::move(hook)) {}

std::optional<std::string> ConsoleInput::readLine() {
    std::string line;
    for (int c = keyboard->sbumpc(); c != traits_type::eof(); c = keyboard->sbumpc()) {
        if (c == '\n') return line;
        if (c != '\r') line += static_cast<char>(c);
    }
    if (line.empty()) return std::nullopt;
    return line;
}

int ConsoleInput::underflow() {
    if (gptr() < egptr()) return traits_type::to_int_type(*gptr());
    while (std::optional<std::string> line = readLine()) {
        if (hook(*line)) continue;
        pending = *line + "\n";
        setg(&pending[0], &pending[0], &pending[0] + pending.size());
        return traits_type::to_int_type(*gptr());
    }
    return traits_type::eof();
}
