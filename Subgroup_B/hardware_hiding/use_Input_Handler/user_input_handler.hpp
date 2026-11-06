#ifndef USER_INPUT_HANDLER_HPP
#define USER_INPUT_HANDLER_HPP

#include <string>
#include <optional>

struct UserInput {
    std::string input;
};

class UserInputHandler {
public:
    virtual ~UserInputHandler() = default;

    virtual std::optional<UserInput> getInput() = 0;
};

class ConsoleInputHandler : public UserInputHandler {
public:
    std::optional<UserInput> getInput() override;
};

#endif