#pragma once

#include <string>

struct User {
    int userId;
    std::string username;
    std::string passwordSalt;
    std::string passwordHash;
    std::string accountRole;
};