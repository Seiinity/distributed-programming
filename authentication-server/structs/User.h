#ifndef USER_H
#define USER_H

#include <string>

#include "../enums/UserType.h"

struct User
{
    std::string username;
    std::string passwordHash;
    UserType type;
    int energy;
    bool isAdmin;

    User() : type(UserType::Freemium), energy(100), isAdmin(false) {}
};

#endif //USER_H
