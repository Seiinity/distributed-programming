#include "JsonHelper.h"
#include <fstream>
#include <iostream>

JsonMessage JsonHelper::parseMessage(const std::string& jsonStr)
{
    JsonMessage msg;

    try
    {
        const json j = json::parse(jsonStr);

        msg.action = j.value("action", "");
        msg.authToken = j.value("token", "");
        msg.username = j.value("username", "");
        msg.itemId = j.value("itemId", "");
        msg.targetUser = j.value("targetUser", "");
        msg.newType = j.value("newType", "");
        msg.message = j.value("message", "");
        msg.success = j.value("success", false);

    }
    catch (const json::parse_error& e)
    {
        std::cout << "JSON parse error: " << e.what() << std::endl;
    }

    return msg;
}

std::string JsonHelper::createResponse(bool success, const std::string& message, const json& data)
{
    json response;
    response["success"] = success;
    response["message"] = message;

    if (!data.empty()) { response["data"] = data; }
    return response.dump();
}

json JsonHelper::itemToJson(const ItemInstance& itemInstance)
{
    json j;

    j["id"] = GUIDUtils::GUIDToString(itemInstance.id);
    j["type"] = itemInstance.item.type;
    j["name"] = itemInstance.item.name;
    j["weight"] = itemInstance.item.weight;
    j["value"] = itemInstance.item.value;

    return j;
}

ItemInstance JsonHelper::jsonToItem(const json& j)
{
    Item item;

    item.type = j.value("type", "");
    item.name = j.value("name", "");
    item.weight = j.value("weight", 0);
    item.value = j.value("value", 0.0f);
    const std::string guidStr = j.value("id", "");

    ItemInstance itemInstance(item, GUIDUtils::stringToGUID(guidStr));
    return itemInstance;
}

json JsonHelper::playerToJson(const Player& player)
{
    json j;
    j["username"] = player.username;
    j["type"] = playerTypeToString(player.type);
    j["balance"] = player.balance;
    j["is_admin"] = player.isAdmin;

    j["inventory"] = json::array();
    for (const auto& itemInstance : player.inventory)
    {
        j["inventory"].push_back(itemToJson(itemInstance));
    }

    return j;
}

Player JsonHelper::jsonToPlayer(const json& j)
{
    Player player;
    player.username = j.value("username", "");
    player.type = stringToPlayerType(j.value("type", "")).value_or(PlayerType::Freemium);
    player.balance = j.value("balance", 0.0f);
    player.isAdmin = j.value("is_admin", false);

    if (j.contains("inventory") && j["inventory"].is_array())
    {
        for (const auto& itemJson : j["inventory"])
        {
            ItemInstance itemInstance = jsonToItem(itemJson);
            player.collectItem(itemInstance);
        }
    }

    return player;
}

StatusResponse JsonHelper::loadGameDataFromFile(const std::string& filename, std::map<std::string, Player>& players)
{
    std::ifstream file(filename);

    if (!file.is_open())
    {
        return { false, "No existing game data file found. Starting fresh" };
    }

    try
    {
        json j;
        file >> j;
        file.close();

        players.clear();

        if (!j.is_array())
        {
            return { false, "Invalid JSON format: expected array" };
        }

        for (const auto& playerJson : j)
        {
            if (playerJson.is_null() || !playerJson.is_object())
            {
                continue;
            }

            try
            {
                if (Player player = jsonToPlayer(playerJson); !player.username.empty())
                {
                    players[player.username] = player;
                }
            }
            catch (const std::exception& e)
            {
                std::cout << e.what() << std::endl;
            }
        }

        return { true, "Loaded " + std::to_string(players.size()) + " players from file" };
    }
    catch (const json::parse_error& e)
    {
        return { false, "Error parsing game data file: " + std::string(e.what()) };
    }
    catch (const std::exception& e)
    {
        return { false, "Error loading game data file: " + std::string(e.what()) };
    }
}

StatusResponse JsonHelper::saveGameDataToFile(const std::string& filename, const std::map<std::string, Player>& players)
{
    try
    {
        json j = json::array();
        for (const auto& player : players | std::views::values)
        {
            j.push_back(playerToJson(player));
        }

        std::ofstream file(filename);
        if (!file.is_open())
        {
            return { false, "Error: Could not save players to file!" };
        }

        file << j.dump(2);
        file.close();

        return { true, "Saved " + std::to_string(players.size()) + " players to file" };
    }
    catch (const std::exception& e)
    {
        return { false, "Error saving players: " + std::string(e.what()) };
    }
}
