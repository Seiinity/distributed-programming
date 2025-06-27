#ifndef JSONHELPER_H
#define JSONHELPER_H

#include <map>
#include <string>
#include <nlohmann/json.hpp>

#include "../structs/JsonMessage.h"
#include "../structs/StatusResponse.h"
#include "../structs/User.h"

using json = nlohmann::json;

class JsonHelper
{
public:

    // Parses incoming JSON message.
    static JsonMessage parseMessage(const std::string& jsonStr);

    // Creates a JSON response.
    static std::string createResponse(bool success, const std::string& message, const std::string& token = "");

    // Creates a JSON response with extra data.
    static std::string createResponse(bool success, const std::string& message, const std::string& token, const json &data);

    // Loads users from JSON file.
    static StatusResponse loadUsersFromFile(const std::string& filename, std::map<std::string, User>& users);

    // Saves users to JSON file.
    static StatusResponse saveUsersToFile(const std::string& filename, const std::map<std::string, User>& users);
};

#endif //JSONHELPER_H
