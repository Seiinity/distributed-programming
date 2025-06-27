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
        msg.username = j.value("username", "");
        msg.password = j.value("password", "");
        msg.authToken = j.value("token", "");
        msg.targetUser = j.value("target_user", "");
        msg.newType = j.value("new_type", "");
    }
    catch (const json::parse_error& e)
    {
        std::cout << "JSON parse error: " << e.what() << std::endl;
    }

    return msg;
}

std::string JsonHelper::createResponse(bool success, const std::string& message, const std::string& token)
{
    json response;
    response["success"] = success;
    response["message"] = message;

    if (!token.empty()) { response["token"] = token; }

    return response.dump();
}

std::string JsonHelper::createResponse(bool success, const std::string &message, const std::string &token, const json &data)
{
    json response;
    response["success"] = success;
    response["message"] = message;
    response["token"] = token;
    response["data"] = data;

    return response.dump();
}

StatusResponse JsonHelper::loadUsersFromFile(const std::string& filename, std::map<std::string, User>& users)
{
    std::ifstream file(filename);

    if (!file.is_open())
    {
        return { false, "No existing user data file found. Starting fresh" };
    }

    try
    {
        json j;
        file >> j;
        file.close();

        if (j.is_array())
        {
            for (const auto& userJson : j)
            {
                User user;
                user.username = userJson.value("username", "");
                user.passwordHash = userJson.value("passwordHash", "");
                user.type = stringToUserType(userJson.value("type", "")).value_or(UserType::Freemium);
                user.energy = userJson.value("energy", 100);
                user.isAdmin = userJson.value("is_admin", false);

                if (!user.username.empty()) { users[user.username] = user; }
            }
        }

        return { true, "Loaded " + std::to_string(users.size()) + " users from file" };

    }
    catch (const json::parse_error& e)
    {
        file.close();
        return { false, "Error parsing user data file: " + std::string(e.what()) };
    }
    catch (const std::exception& e)
    {
        file.close();
        return { false, "Error parsing user data: " + std::string(e.what()) };
    }
}

StatusResponse JsonHelper::saveUsersToFile(const std::string& filename, const std::map<std::string, User>& users)
{
    try
    {
        json j = json::array();

        for (const auto& val : users | std::views::values)
        {
            const User& user = val;
            json userJson;
            userJson["username"] = user.username;
            userJson["passwordHash"] = user.passwordHash;
            userJson["type"] = userTypeToString(user.type);
            userJson["energy"] = user.energy;
            userJson["is_admin"] = user.isAdmin;

            j.push_back(userJson);
        }

        std::ofstream file(filename);

        if (!file.is_open())
        {
            return { false, "Error: Could not save users to file!" };
        }

        file << j.dump(2);
        file.close();

        return { true, "Saved " + std::to_string(users.size()) + " users to file" };
    }
    catch (const std::exception& e)
    {
        return { false, "Error saving user data: " + std::string(e.what()) };
    }
}
