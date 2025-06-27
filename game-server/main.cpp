#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <vector>
#include <string>
#include <map>
#include <atomic>
#include <csignal>
#include <random>

#include "AuthServerClient.h"
#include "structs/Player.h"
#include "utilities/JsonHelper.h"

namespace
{
    const std::string GAME_DATA_FILE = "game_data.json";

    // Some sample items for adventures.
    const std::vector ADVENTURE_ITEMS =
    {
        Item("weapon", "Iron Dagger", 3, 150.0),
        Item("consumable", "Health Potion", 1, 25.0),
        Item("armor", "Leather Armour", 5, 200.0),
        Item("treasure", "Amethyst", 1, 300.0),
        Item("equipment", "Wooden Shield", 2, 75.0),
        Item("weapon", "Compound Bow", 4, 180.0),
        Item("accessory", "Silver Ring", 1, 120.0)
    };

    AuthServerClient authClient;

    std::atomic currentConnections{0};
    std::mutex connectionMutex;

    std::map<std::string, Player> players;
    std::map<std::string, bool> onlineUsers;
    std::mutex onlineUsersMutex; // Thread safety for online users map.

    std::map<std::string, ItemInstance> pendingItems;
    std::atomic serverRunning { true };
    SOCKET listenSocket = INVALID_SOCKET;
    std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());

    // Shuts the server down gracefully.
    void performShutdown()
    {
        if (!serverRunning.exchange(false)) return; // Prevents multiple shutdowns.

        std::cout << "\nShutting down the game server..." << std::endl;

        const StatusResponse status = JsonHelper::saveGameDataToFile(GAME_DATA_FILE, players);
        std::cout << status.message << std::endl;

        if (listenSocket != INVALID_SOCKET)
        {
            closesocket(listenSocket);
            listenSocket = INVALID_SOCKET;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        WSACleanup();
        std::cout << "Game server shutdown complete." << std::endl;
    }

    // Signal handler for graceful shutdown.
    void signalHandler(int signal)
    {
        performShutdown();
        exit(0);
    }

    // Windows console API handler (click X, stop debugging, etc.).
    BOOL WINAPI ConsoleHandler(const DWORD dwCtrlType)
    {
        switch (dwCtrlType)
        {
        case CTRL_C_EVENT:
            std::cout << "\nCtrl+C received" << std::endl;
            break;
        case CTRL_CLOSE_EVENT:
            std::cout << "\nConsole window closing" << std::endl;
            break;
        case CTRL_BREAK_EVENT:
            std::cout << "\nCtrl+Break received" << std::endl;
            break;
        case CTRL_LOGOFF_EVENT:
            std::cout << "\nUser is logging off" << std::endl;
            break;
        case CTRL_SHUTDOWN_EVENT:
            std::cout << "\nSystem is shutting down" << std::endl;
            break;
        default:
            break;
        }

        performShutdown();

        if (dwCtrlType == CTRL_CLOSE_EVENT)
        {
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }

        return TRUE;
    }

    // Creates an admin account if none is present at server initialisation.
    void ensureAdminPlayerExists()
    {
        const std::string adminUsername = "admin";

        if (players.contains(adminUsername))
        {
            Player& adminPlayer = players[adminUsername];
            adminPlayer.isAdmin = true;
            adminPlayer.balance = 0.0f;
            adminPlayer.inventory.clear();
            return;
        }

        Player adminPlayer;
        adminPlayer.username = adminUsername;
        adminPlayer.authToken = "";
        adminPlayer.type = PlayerType::Freemium;
        adminPlayer.balance = 0.0f;
        adminPlayer.isAdmin = true;

        players[adminUsername] = adminPlayer;
    }

    // Checks if the player can join or if the server is full.
    bool canUserConnect(const std::string& username, const std::string& token)
    {
        json request;
        request["action"] = "get_user_info";
        request["username"] = username;
        request["token"] = token;

        const std::string jsonRequest = request.dump();
        std::string response = authClient.sendRequest(jsonRequest);

        if (response.empty() && !authClient.isConnectionValid())
        {
            if (authClient.reconnect())
            {
                response = authClient.sendRequest(jsonRequest);
            }
        }

        if (response.empty())
        {
            std::cout << "Failed to get user info for connection limit check" << std::endl;
            return false;
        }

        try
        {
            json responseJson = json::parse(response);

            if (const bool success = responseJson.value("success", false); success && responseJson.contains("data"))
            {
                const json data = responseJson["data"];
                const int userConnectionLimit = data.value("connection_limit", 50); // Defaults to Freemium.

                std::lock_guard lock(connectionMutex);
                const int current = currentConnections.load();

                std::cout << "Connection check for " << username << ": "
                          << current << "/" << userConnectionLimit
                          << " (Type: " << data.value("type", "Freemium") << ")" << std::endl;

                return current < userConnectionLimit;
            }

        }
        catch (const json::parse_error& e)
        {
            std::cout << "Failed to parse user info for connection limit: " << e.what() << std::endl;
        }

        return false; // Deny access if we can't determine limits
    }

    // Increments the connection count.
    void incrementConnections()
    {
        std::lock_guard lock(connectionMutex);
        ++currentConnections;
        std::cout << "Total connections: " << currentConnections.load() << std::endl;
    }

    // Decrements the connection count-
    void decrementConnections()
    {
        std::lock_guard lock(connectionMutex);
        if (currentConnections > 0) { --currentConnections; }
        std::cout << "Total connections: " << currentConnections.load() << std::endl;
    }

    // Validates the player's auth token (very simplified!).
    bool validateToken(const std::string& token, const std::string& username)
    {
        const std::string expectedPrefix = "AUTH_" + username + "_";
        return token.substr(0, expectedPrefix.length()) == expectedPrefix;
    }

    // Marks a user as being online.
    void markUserOnline(const std::string& username)
    {
        std::lock_guard lock(onlineUsersMutex);
        onlineUsers[username] = true;
        std::cout << "User " << username << " is now online. Total online: " << onlineUsers.size() << std::endl;
    }

    // Marks a user as being offline.
    void markUserOffline(const std::string& username)
    {
        std::lock_guard lock(onlineUsersMutex);
        onlineUsers.erase(username);
        std::cout << "User " << username << " is now offline. Total online: " << onlineUsers.size() << std::endl;
    }

    // Checks if a user is online.
    bool isUserOnline(const std::string& username)
    {
        std::lock_guard lock(onlineUsersMutex);
        return onlineUsers.contains(username);
    }

    // Creates a unique item instance of a random item.
    ItemInstance generateRandomItemInstance()
    {
        std::uniform_int_distribution<> dist(0, static_cast<int>(ADVENTURE_ITEMS.size()) - 1);
        const Item& baseItem = ADVENTURE_ITEMS[dist(rng)];

        return ItemInstance(baseItem);
    }

    // Communicates with the authentication server to check and deduct energy.
    bool checkAndDeductEnergy(const std::string& username, const std::string& token)
    {
        json request;
        request["action"] = "check_energy";
        request["username"] = username;
        request["token"] = token;

        const std::string jsonRequest = request.dump();
        std::cout << "Requesting energy check from auth server for: " << username << std::endl;

        std::string response = authClient.sendRequest(jsonRequest);
        if (response.empty() && !authClient.isConnectionValid())
        {
            if (authClient.reconnect())
            {
                response = authClient.sendRequest(jsonRequest);
            }
        }

        if (response.empty())
        {
            std::cout << "Failed to communicate with the authentication server" << std::endl;
            return false;
        }

        try
        {
            json responseJson = json::parse(response);
            const bool success = responseJson.value("success", false);

            if (success)
            {
                std::cout << "Energy check successful for " << username << std::endl;

                if (responseJson.contains("data"))
                {
                    const json data = responseJson["data"];
                    const int energyCost = data.value("energy_cost", 0);
                    const int remainingEnergy = data.value("remaining_energy", 0);
                    std::cout << "Energy cost: " << energyCost << ", remaining: " << remainingEnergy << std::endl;
                }
            }
            else
            {
                const std::string message = responseJson.value("message", "Unknown error");
                std::cout << "Energy check failed for " << username << ": " << message << std::endl;
            }

            return success;
        }
        catch (const json::parse_error& e)
        {
            std::cout << "Failed to parse auth server response: " << e.what() << std::endl;
            return false;
        }
    }

    // Retrieves user info from the authentication server.
    std::optional<std::string> getUserTypeFromAuthServer(const std::string& username, const std::string& token)
    {
        json request;
        request["action"] = "get_user_info";
        request["username"] = username;
        request["token"] = token;

        const std::string jsonRequest = request.dump();
        std::string response = authClient.sendRequest(jsonRequest);

        if (response.empty() && !authClient.isConnectionValid())
        {
            if (authClient.reconnect())
            {
                response = authClient.sendRequest(jsonRequest);
            }
        }

        if (response.empty())
        {
            return std::nullopt;
        }

        try
        {
            json responseJson = json::parse(response);

            if (const bool success = responseJson.value("success", false); success && responseJson.contains("data"))
            {
                const json data = responseJson["data"];
                return data.value("type", "Freemium");
            }
        }
        catch (const json::parse_error& e)
        {
            std::cout << "Failed to parse user info response: " << e.what() << std::endl;
        }

        return std::nullopt;
    }

    // Handles the 'adventure' command.
    std::string handleAdventure(const std::string& username, const std::string& token)
    {
        if (!validateToken(token, username))
        {
            return JsonHelper::createResponse(false, "Invalid authentication token");
        }

        if (!checkAndDeductEnergy(username, token))
        {
            return JsonHelper::createResponse(false, "Insufficient energy or failed to contact authentication server");
        }

        auto it = players.find(username);
        if (it == players.end())
        {
            Player newPlayer;
            newPlayer.username = username;
            newPlayer.authToken = token;
            newPlayer.balance = 0.0;
            newPlayer.isAdmin = false;
            players[username] = newPlayer;

            // Get the player type from the auth server.
            if (const auto userTypeOpt = getUserTypeFromAuthServer(username, token); userTypeOpt.has_value())
            {
                const auto playerTypeOpt = stringToPlayerType(userTypeOpt.value());
                newPlayer.type = playerTypeOpt.value_or(PlayerType::Freemium);
            }
            else
            {
                newPlayer.type = PlayerType::Freemium; // Default fallback
            }

            it = players.find(username);
        }

        Player& player = it->second;

        // 50% to get an item, 50% to earn money.
        std::uniform_int_distribution chanceDist(1, 100);
        const bool shouldGetItem = chanceDist(rng) <= 50;

        json responseData;

        if (shouldGetItem)
        {
            const ItemInstance itemInstance = generateRandomItemInstance();

            pendingItems[username] = itemInstance;

            responseData["type"] = "item";
            responseData["item"] = JsonHelper::itemToJson(itemInstance);
            responseData["message"] = "Use command 'store' to store this item in your inventory";

            return JsonHelper::createResponse(true, "Adventure complete! Found item: " + itemInstance.item.name, responseData);
        }

        // Player can earn between $15 and $75.
        std::uniform_real_distribution moneyDist(15.0f, 75.0f);
        float money = std::round(moneyDist(rng) * 100.0f) / 100.0f;
        player.balance += money;

        responseData["type"] = "money";
        responseData["amount"] = money;
        responseData["total_balance"] = player.balance;

        std::ostringstream moneyStream;
        moneyStream << std::fixed << std::setprecision(2) << money;

        return JsonHelper::createResponse(true, "Adventure complete! Found money: $" + moneyStream.str(), responseData);
    }

    // Handles the 'store' command.
    std::string handleStore(const std::string& username, const std::string& token)
    {
        if (!validateToken(token, username))
        {
            return JsonHelper::createResponse(false, "Invalid authentication token");
        }

        const auto pendingIt = pendingItems.find(username);
        if (pendingIt == pendingItems.end())
        {
            return JsonHelper::createResponse(false, "No item to store");
        }

        const auto playerIt = players.find(username);
        if (playerIt == players.end())
        {
            return JsonHelper::createResponse(false, "You must go on an adventure first");
        }

        Player& player = playerIt->second;
        ItemInstance itemToStore = pendingIt->second;

        if (!player.canAddItem(itemToStore.item))
        {
            json responseData;
            responseData["used_space"] = player.getUsedInventorySpace();
            responseData["max_space"] = player.getMaxInventorySpace();
            responseData["item_space"] = itemToStore.item.weight;
            responseData["item_name"] = itemToStore.item.name;
            responseData["item_type"] = itemToStore.item.type;

            return JsonHelper::createResponse(false, "Not enough inventory space to store item", responseData);
        }

        player.collectItem(itemToStore);
        pendingItems.erase(pendingIt);

        json responseData;
        responseData["used_space"] = player.getUsedInventorySpace();
        responseData["max_space"] = player.getMaxInventorySpace();
        responseData["item_name"] = itemToStore.item.name;
        responseData["item_type"] = itemToStore.item.type;
        responseData["item_weight"] = itemToStore.item.weight;
        responseData["item_value"] = itemToStore.item.value;

        return JsonHelper::createResponse(true, "Item stored successfully: " + itemToStore.item.name + " [ID: " + GUIDUtils::GUIDToString(itemToStore.id) + "]", responseData);
    }

    // Handles the 'remove' command.
    std::string handleRemove(const std::string& username, const std::string& token, const std::string& itemId)
    {
        if (!validateToken(token, username))
        {
            return JsonHelper::createResponse(false, "Invalid authentication token");
        }

        const auto playerIt = players.find(username);
        if (playerIt == players.end())
        {
            return JsonHelper::createResponse(false, "Player not found");
        }

        Player& player = playerIt->second;
        const std::optional<ItemInstance> removedItemOptional = player.getItemFromInventory(itemId);

        if (!removedItemOptional.has_value())
        {
            return JsonHelper::createResponse(false, "Item not found in inventory");
        }

        const ItemInstance& removedItem = removedItemOptional.value();
        player.dropItem(removedItem);

        json responseData;
        responseData["removed_item"] = JsonHelper::itemToJson(removedItem);
        responseData["used_space"] = player.getUsedInventorySpace();
        responseData["max_space"] = player.getMaxInventorySpace();

        return JsonHelper::createResponse(true, "Item removed successfully: " + removedItem.item.name, responseData);
    }

    // Handles the 'sell' command.
    std::string handleSell(const std::string& username, const std::string& token, const std::string& itemId)
    {
        if (!validateToken(token, username))
        {
            return JsonHelper::createResponse(false, "Invalid authentication token");
        }

        const auto playerIt = players.find(username);
        if (playerIt == players.end())
        {
            return JsonHelper::createResponse(false, "Player not found");
        }

        Player& player = playerIt->second;
        const std::optional<ItemInstance> soldItemOptional = player.getItemFromInventory(itemId);

        if (!soldItemOptional.has_value())
        {
            return JsonHelper::createResponse(false, "Item not found in inventory");
        }

        ItemInstance soldItem = soldItemOptional.value();
        player.balance += soldItem.item.value;
        player.dropItem(soldItem);

        json responseData;
        responseData["sold_item"] = JsonHelper::itemToJson(soldItem);
        responseData["item_value"] = soldItem.item.value;
        responseData["new_balance"] = player.balance;
        responseData["used_space"] = player.getUsedInventorySpace();
        responseData["max_space"] = player.getMaxInventorySpace();

        std::ostringstream stream;
        stream << std::fixed << std::setprecision(2) << soldItem.item.value;

        return JsonHelper::createResponse(true, "Item sold successfully: " + soldItem.item.name + " for $" + stream.str(), responseData);
    }

    // Handles the 'list_items' command.
    std::string handleListItems(const std::string& username, const std::string& token)
    {
        if (!validateToken(token, username))
        {
            return JsonHelper::createResponse(false, "Invalid authentication token");
        }

        const auto playerIt = players.find(username);
        if (playerIt == players.end())
        {
            return JsonHelper::createResponse(false, "Player not found");
        }

        const Player& player = playerIt->second;

        json responseData;
        responseData["inventory"] = json::array();

        for (const auto& itemInstance : player.inventory)
        {
            responseData["inventory"].push_back(JsonHelper::itemToJson(itemInstance));
        }

        responseData["total_items"] = player.inventory.size();
        responseData["used_space"] = player.getUsedInventorySpace();
        responseData["max_space"] = player.getMaxInventorySpace();

        return JsonHelper::createResponse(true, "Inventory retrieved successfully", responseData);
    }

    // Handles the 'space' command.
    std::string handleSpace(const std::string& username, const std::string& token)
    {
        if (!validateToken(token, username))
        {
            return JsonHelper::createResponse(false, "Invalid authentication token");
        }

        const auto playerIt = players.find(username);
        if (playerIt == players.end())
        {
            return JsonHelper::createResponse(false, "Player not found");
        }

        const Player& player = playerIt->second;

        json responseData;
        responseData["used_space"] = player.getUsedInventorySpace();
        responseData["max_space"] = player.getMaxInventorySpace();
        responseData["available_space"] = player.getMaxInventorySpace() - player.getUsedInventorySpace();
        responseData["player_type"] = player.type;

        return JsonHelper::createResponse(true, "Space: " + std::to_string(player.getUsedInventorySpace()) + "/" + std::to_string(player.getMaxInventorySpace()), responseData);
    }

    // Handles the 'list_users' command.
    std::string handleListUsers(const std::string& username, const std::string& token)
    {
        if (!validateToken(token, username))
        {
            return JsonHelper::createResponse(false, "Invalid authentication token");
        }

        const auto playerIt = players.find(username);
        if (playerIt == players.end())
        {
            return JsonHelper::createResponse(false, "Player not found");
        }

        const Player& player = playerIt->second;

        json responseData;
        responseData["users"] = json::array();

        if (player.isAdmin)
        {
            // The admin can see all registered users.
            for (const auto& playerData : players | std::views::values)
            {
                json userInfo;
                userInfo["username"] = playerData.username;
                userInfo["is_online"] = isUserOnline(playerData.username);
                responseData["users"].push_back(userInfo);
            }

            return JsonHelper::createResponse(true, "All users retrieved (admin view)", responseData);
        }

        // Regular users can only see currently online users.
        std::lock_guard lock(onlineUsersMutex);

        for (const auto& onlineUsername : onlineUsers | std::views::keys)
        {
            if (players.contains(onlineUsername))
            {
                json userInfo;
                userInfo["username"] = onlineUsername;
                responseData["users"].push_back(userInfo);
            }
        }

        return JsonHelper::createResponse(true, "Online users retrieved", responseData);
    }

    // Handles the 'modify_type' command.
    std::string handleModifyType(const std::string& username, const std::string& token, const std::string& targetUser, const std::string& newType)
    {
        if (!validateToken(token, username))
        {
            return JsonHelper::createResponse(false, "Invalid authentication token");
        }

        const auto adminIt = players.find(username);
        if (adminIt == players.end())
        {
            return JsonHelper::createResponse(false, "Player not found");
        }

        if (!adminIt->second.isAdmin)
        {
            return JsonHelper::createResponse(false, "Insufficient permissions. Admin access required");
        }

        const auto targetIt = players.find(targetUser);
        if (targetIt == players.end())
        {
            return JsonHelper::createResponse(false, "Target user not found");
        }

        const std::optional<PlayerType> newPlayerTypeOpt = stringToPlayerType(newType);
        if (!newPlayerTypeOpt.has_value())
        {
            return JsonHelper::createResponse(false, "Invalid player type. Valid types: Freemium, Bronze, Silver, Gold, Platinum");
        }

        PlayerType newPlayerType = newPlayerTypeOpt.value();

        // First, tries to change the type of the user from the authentication server.
        json authRequest;
        authRequest["action"] = "modify_type";
        authRequest["username"] = username;
        authRequest["token"] = token;
        authRequest["target_user"] = targetUser;
        authRequest["new_type"] = playerTypeToString(newPlayerType);

        const std::string jsonRequest = authRequest.dump();
        std::string authResponse = authClient.sendRequest(jsonRequest);

        if (authResponse.empty() && !authClient.isConnectionValid())
        {
            if (authClient.reconnect())
            {
                authResponse = authClient.sendRequest(jsonRequest);
            }
        }

        if (!authResponse.empty())
        {
            try
            {
                bool modifyTypeSuccessful = false;
                const json responseJson = json::parse(authResponse);
                modifyTypeSuccessful = responseJson.value("success", false);

                if (!modifyTypeSuccessful)
                {
                    const std::string authMessage = responseJson.value("message", "Unknown error from auth server");
                    return JsonHelper::createResponse(false, "Failed to modify type of user from auth server: " + authMessage);
                }
            }
            catch (const json::parse_error& e)
            {
                std::cout << "Failed to parse auth server response: " << e.what() << std::endl;
                return JsonHelper::createResponse(false, "Failed to parse auth server response");
            }
        }
        else
        {
            return JsonHelper::createResponse(false, "Failed to communicate with authentication server");
        }

        targetIt->second.type = newPlayerType;

        json responseData;
        responseData["target_user"] = targetUser;
        responseData["new_type"] = playerTypeToString(newPlayerType);

        std::cout << "User " << targetUser << "'s type changed to " << newType << std::endl;
        return JsonHelper::createResponse(true, "User type modified successfully on both servers: " + targetUser + " -> " + newType, responseData);
    }

    // Handles the 'remove_user' command.
    std::string handleRemoveUser(const std::string& username, const std::string& token, const std::string& targetUser)
    {
        if (!validateToken(token, username))
        {
            return JsonHelper::createResponse(false, "Invalid authentication token");
        }

        const auto adminIt = players.find(username);
        if (adminIt == players.end())
        {
            return JsonHelper::createResponse(false, "Player not found");
        }

        if (!adminIt->second.isAdmin)
        {
            return JsonHelper::createResponse(false, "Insufficient permissions. Admin access required");
        }

        if (targetUser == username)
        {
            return JsonHelper::createResponse(false, "You may not remove yourself");
        }

        const auto targetIt = players.find(targetUser);
        if (targetIt == players.end())
        {
            return JsonHelper::createResponse(false, "Target user not found");
        }

        // First, tries to remove the user from the authentication server.
        json authRequest;
        authRequest["action"] = "remove_user";
        authRequest["username"] = username;
        authRequest["token"] = token;
        authRequest["target_user"] = targetUser;

        const std::string jsonRequest = authRequest.dump();
        std::string authResponse = authClient.sendRequest(jsonRequest);

        if (authResponse.empty() && !authClient.isConnectionValid())
        {
            if (authClient.reconnect())
            {
                authResponse = authClient.sendRequest(jsonRequest);
            }
        }

        if (!authResponse.empty())
        {
            try
            {
                bool authRemovalSuccessful = false;
                const json responseJson = json::parse(authResponse);
                authRemovalSuccessful = responseJson.value("success", false);

                if (!authRemovalSuccessful)
                {
                    const std::string authMessage = responseJson.value("message", "Unknown error from auth server");
                    return JsonHelper::createResponse(false, "Failed to remove user from auth server: " + authMessage);
                }
            }
            catch (const json::parse_error& e)
            {
                std::cout << "Failed to parse auth server response: " << e.what() << std::endl;
                return JsonHelper::createResponse(false, "Failed to parse auth server response");
            }
        }
        else
        {
            return JsonHelper::createResponse(false, "Failed to communicate with authentication server");
        }

        markUserOffline(targetUser);
        players.erase(targetIt);

        json responseData;
        responseData["removed_user"] = targetUser;
        responseData["remaining_users"] = players.size();

        std::cout << "User " << targetUser << " removed from both auth server and game server by " << username << std::endl;
        return JsonHelper::createResponse(true, "User removed successfully from both servers: " + targetUser, responseData);
    }

    // Handles client connections.
    void handleClient(const SOCKET clientSocket, const int clientId)
    {
        std::cout << "Client " << clientId << " connected!" << std::endl;

        char buffer[1024];
        std::string messageBuffer;
        std::string connectedUsername;
        bool connectionApproved = false;



        markUserOnline(connectedUsername);

        while (serverRunning)
        {
            if (const int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0); bytesReceived > 0)
            {
                buffer[bytesReceived] = '\0';
                messageBuffer += std::string(buffer);

                // Looks for the complete JSON message.
                if (const size_t pos = messageBuffer.find('}'); pos != std::string::npos)
                {
                    std::string completeMessage = messageBuffer.substr(0, pos + 1);
                    messageBuffer = messageBuffer.substr(pos + 1);

                    std::cout << "Client " << clientId << " sent: " << completeMessage << std::endl;

                    // Parses messages sent by the user.
                    JsonMessage msg = JsonHelper::parseMessage(completeMessage);
                    std::string response;

                    if (!connectionApproved && !msg.username.empty() && validateToken(msg.authToken, msg.username))
                    {
                        if (!canUserConnect(msg.username, msg.authToken)) // Admin can always connect.
                        {
                            response = JsonHelper::createResponse(false, "Server is at capacity for your user type. Please try again later");
                            send(clientSocket, response.c_str(), static_cast<int>(response.length()), 0);
                            std::cout << "Connection rejected for " << msg.username << " - server at capacity" << std::endl;
                            break;
                        }

                        // Connection was approved! :D
                        connectionApproved = true;
                        connectedUsername = msg.username;
                        markUserOnline(connectedUsername);
                        incrementConnections();
                        std::cout << "Connection approved for " << msg.username << std::endl;
                    }

                    if (!connectionApproved)
                    {
                        response = JsonHelper::createResponse(false, "Please authenticate first");
                    }
                    else
                    {
                        if (msg.action == "adventure")
                        {
                            if (connectedUsername == "admin")
                            {
                                response = JsonHelper::createResponse(false, "Admin accounts cannot go on adventures");
                            }
                            else
                            {
                                response = handleAdventure(msg.username, msg.authToken);
                            }
                        }
                        else if (msg.action == "store")
                        {
                            if (connectedUsername == "admin")
                            {
                                response = JsonHelper::createResponse(false, "Admin accounts have no inventory");
                            }
                            else
                            {
                                response = handleStore(msg.username, msg.authToken);
                            }
                        }
                        else if (msg.action == "remove")
                        {
                            if (connectedUsername == "admin")
                            {
                                response = JsonHelper::createResponse(false, "Admin accounts have no inventory");
                            }
                            else
                            {
                                response = handleRemove(msg.username, msg.authToken, msg.itemId);
                            }
                        }
                        else if (msg.action == "sell")
                        {
                            if (connectedUsername == "admin")
                            {
                                response = JsonHelper::createResponse(false, "Admin accounts have no inventory");
                            }
                            else
                            {
                                response = handleSell(msg.username, msg.authToken, msg.itemId);
                            }
                        }
                        else if (msg.action == "list_items")
                        {
                            if (connectedUsername == "admin")
                            {
                                response = JsonHelper::createResponse(false, "Admin accounts have no inventory");
                            }
                            else
                            {
                                response = handleListItems(msg.username, msg.authToken);
                            }
                        }
                        else if (msg.action == "space")
                        {
                            if (connectedUsername == "admin")
                            {
                                response = JsonHelper::createResponse(false, "Admin accounts have no inventory");
                            }
                            else
                            {
                                response = handleSpace(msg.username, msg.authToken);
                            }
                        }
                        else if (msg.action == "list_users")
                        {
                            response = handleListUsers(msg.username, msg.authToken);
                        }
                        else if (msg.action == "modify_type")
                        {
                            response = handleModifyType(msg.username, msg.authToken, msg.targetUser, msg.newType);
                        }
                        else if (msg.action == "remove_user")
                        {
                            response = handleRemoveUser(msg.username, msg.authToken, msg.targetUser);
                        }
                        else
                        {
                            response = JsonHelper::createResponse(false, "Unknown action: " + msg.action);
                        }
                    }

                    // Sends the response back.
                    send(clientSocket, response.c_str(), static_cast<int>(response.length()), 0);
                }
            }
            else if (bytesReceived == 0)
            {
                std::cout << "Client " << clientId << " disconnected" << std::endl;
                break;
            }
            else
            {
                // Checks for specific error codes.
                if (const int error = WSAGetLastError(); error == WSAECONNRESET || error == 10054)
                {
                    std::cout << "Client " << clientId << " disconnected (connection reset)" << std::endl;
                }
                else if (error == WSAECONNABORTED || error == 10053)
                {
                    std::cout << "Client " << clientId << " disconnected (connection aborted)" << std::endl;
                }
                else if (error == WSAESHUTDOWN || error == 10058)
                {
                    std::cout << "Client " << clientId << " disconnected (socket shutdown)" << std::endl;
                }
                else if (error == WSAETIMEDOUT || error == 10060)
                {
                    std::cout << "Client " << clientId << " disconnected (timeout)" << std::endl;
                }
                else
                {
                    std::cout << "Receiving data failed for client " << clientId << ": " << error << std::endl;
                }
                break;
            }
        }

        if (connectionApproved)
        {
            decrementConnections();
        }

        if (!connectedUsername.empty())
        {
            markUserOffline(connectedUsername);
        }

        closesocket(clientSocket);
    }
}

int main(int argc, char* argv[])
{
    if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE))
    {
        std::cout << "Warning: Could not set console control handler" << std::endl;
    }

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Initialises Winsock (version 2.2).
    WSADATA wsaData;
    if (const int result = WSAStartup(MAKEWORD(2, 2), &wsaData); result != 0)
    {
        std::cout << "WSAStartup failed: " << result << std::endl;
        ExitProcess(EXIT_FAILURE);
    }

    std::cout << "Winsock initialized successfully" << std::endl;

    // Attempts to connect to the authentication server before starting.
    std::cout << "\n=== AUTHENTICATION SERVER DEPENDENCY ===" << std::endl;
    if (!authClient.connect())
    {
        std::cout << "\nSTARTUP FAILED!" << std::endl;
        std::cout << "The game server requires the authentication server to be running." << std::endl;
        std::cout << "\nPlease:" << std::endl;
        std::cout << "1. Start the authentication server on port 8080" << std::endl;
        std::cout << "2. Verify that it is accepting connections" << std::endl;
        std::cout << "3. Restart the game server" << std::endl;

        WSACleanup();
        ExitProcess(EXIT_FAILURE);
    }

    // Loads the game data.
    std::cout << "Loading game data..." << std::endl;
    const StatusResponse status = JsonHelper::loadGameDataFromFile(GAME_DATA_FILE, players);
    std::cout << status.message << "\n" << std::endl;

    ensureAdminPlayerExists();

    // Creates a socket with the TCP protocol for listening.
    listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET)
    {
        std::cout << "Socket creation failed: " << WSAGetLastError() << std::endl;
        authClient.disconnect();
        WSACleanup();
        ExitProcess(EXIT_FAILURE);
    }

    // Configures the server address and port.
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(8081);

    // Binds the listening socket to the configured address.
    if (const int bindResult = bind(listenSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)); bindResult == SOCKET_ERROR)
    {
        std::cout << "Bind failed: " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        authClient.disconnect();
        WSACleanup();
        ExitProcess(EXIT_FAILURE);
    }

    std::cout << "Socket bound to port 8081" << std::endl;

    // Starts listening for incoming connections.
    if (const int listenResult = listen(listenSocket, SOMAXCONN); listenResult == SOCKET_ERROR)
    {
        std::cout << "Listen failed: " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        authClient.disconnect();
        WSACleanup();
        ExitProcess(EXIT_FAILURE);
    }

    std::cout << "\n==================================" << std::endl;
    std::cout << "Game Server listening on port 8081" << std::endl;
    std::cout << "==================================\n" << std::endl;

    std::cout << "Press Ctrl+C to shutdown server gracefully" << std::endl;

    // Keeps track of all client threads.
    std::vector<std::thread> clientThreads;
    int clientCounter = 0;

    // Main server loop - accepts multiple clients.
    while (serverRunning)
    {
        const SOCKET clientSocket = accept(listenSocket, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET)
        {
            std::cout << "Accept failed: " << WSAGetLastError() << std::endl;
            continue;
        }

        // Creates a new thread for this client.
        clientCounter++;
        clientThreads.emplace_back(handleClient, clientSocket, clientCounter);

        // Detaches the thread so it runs independently.
        clientThreads.back().detach();
    }

    authClient.disconnect();
    return 0;
}