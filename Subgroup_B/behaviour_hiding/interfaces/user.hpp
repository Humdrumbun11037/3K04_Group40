//user (shared type only)

/*
Owned by user information and login. This file only states what the behaviour
modules need to know about the logged-in user: who they are and their role.
*/
#pragma once

#include <string>

namespace hms::behaviour {

enum class Role { Pharmacist, Technician, Administrator };

struct User {
    std::string name;
    Role role;
};

}
