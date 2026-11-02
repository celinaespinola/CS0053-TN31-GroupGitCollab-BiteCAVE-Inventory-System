#pragma once

#include <string>
#include <vector>
#include "user.h"

bool initializeAdministrator(
    const std::string& filename,
    std::string& error
);

bool loadUsers(
    std::vector<User>& users,
    const std::string& filename,
    std::string& error
);

bool authenticateUser(
    const std::vector<User>& users,
    const std::string& username,
    const std::string& password
);