#ifndef JSONMESSAGE_H
#define JSONMESSAGE_H

#include <string>

struct JsonMessage
{
    std::string action;
    std::string username;
    std::string password;
    std::string authToken;
    std::string response;
    std::string targetUser;
    std::string newType;
    bool success;

    JsonMessage() : success(false) {}
};

#endif //JSONMESSAGE_H
