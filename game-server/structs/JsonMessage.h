#ifndef MESSAGE_H
#define MESSAGE_H

#include <string>

struct JsonMessage
{
    std::string action;
    std::string authToken;
    std::string username;
    std::string itemId;
    std::string targetUser;
    std::string newType;
    std::string message;
    bool success;

    JsonMessage() : success(false) {}
};

#endif //MESSAGE_H
