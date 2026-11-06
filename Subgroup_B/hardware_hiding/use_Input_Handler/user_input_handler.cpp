#include "user_input_handler.hpp"

#include <iostream>

std::optional<UserInput> ConsoleInputHandler::getInput()
{
    UserInput input;

    std::cout << "Enter anything: ";

    if (!std::getline(std::cin, input.input)) {
        return std::nullopt;
    }

    return input;
}