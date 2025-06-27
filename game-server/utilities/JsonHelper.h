#ifndef JSONHELPER_H
#define JSONHELPER_H

#include <map>
#include <nlohmann/json.hpp>

#include "../structs/JsonMessage.h"
#include "../structs/Item.h"
#include "../structs/Player.h"
#include "../structs/StatusResponse.h"

using json = nlohmann::json;

class JsonHelper
{
public:

    // Parses an incoming message.
    static JsonMessage parseMessage(const std::string& jsonStr);

    // Creates a response.
    static std::string createResponse(bool success, const std::string& message, const json& data = json::object());

    // Converts Item to JSON.
    static json itemToJson(const ItemInstance& item);

    // Converts JSON to Item.
    static ItemInstance jsonToItem(const json& j);

    // Converts Player to JSON.
    static json playerToJson(const Player& player);

    // Converts JSON to Player.
    static Player jsonToPlayer(const json& j);

    // Loads game data from a file.
    static StatusResponse loadGameDataFromFile(const std::string& filename, std::map<std::string, Player>& players);

    // Saves game data to a file.
    static StatusResponse saveGameDataToFile(const std::string& filename, const std::map<std::string, Player>& players);
};

#endif //JSONHELPER_H
