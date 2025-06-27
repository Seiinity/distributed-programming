#include "GameClient.h"

#include <iostream>
#include <ws2tcpip.h>

#include "utilities/Utilities.h"

GameClient::~GameClient()
{
    disconnect();
}

bool GameClient::connectToAuthServer()
{
    authSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (authSocket == INVALID_SOCKET)
    {
        std::cout << "Failed to create the authentication server socket" << std::endl;
        return false;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

    if (connect(authSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR)
    {
        std::cout << "Failed to connect to the authentication server: " << WSAGetLastError() << std::endl;
        closesocket(authSocket);
        authSocket = INVALID_SOCKET;
        return false;
    }

    std::cout << "Connected to the authentication server" << std::endl;
    return true;
}

bool GameClient::connectToGameServer()
{
    gameSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (gameSocket == INVALID_SOCKET)
    {
        std::cout << "Failed to create the game server socket" << std::endl;
        return false;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8081);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

    if (connect(gameSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR)
    {
        std::cout << "Failed to connect to the game server: " << WSAGetLastError() << std::endl;
        closesocket(gameSocket);
        gameSocket = INVALID_SOCKET;
        return false;
    }

    std::cout << "Connected to the game server" << std::endl;
    return true;
}

std::string GameClient::sendRequest(const SOCKET socket, const json &request, const std::string &serverType)
{
    if (socket == INVALID_SOCKET)
    {
        return "ERROR: Not connected to the " + serverType + " server";
    }

    const std::string jsonStr = request.dump();

    if (send(socket, jsonStr.c_str(), static_cast<int>(jsonStr.length()), 0) == SOCKET_ERROR)
    {
        return "ERROR: Failed to send to the " + serverType + " server";
    }

    char buffer[2048];
    if (const int bytesReceived = recv(socket, buffer, sizeof(buffer) - 1, 0); bytesReceived > 0)
    {
        buffer[bytesReceived] = '\0';
        return { buffer };
    }

    return "ERROR: No response from the " + serverType + " server";
}

std::string GameClient::sendAuthRequest(const json &request) const
{
    return sendRequest(authSocket, request, "authentication");
}

std::string GameClient::sendGameRequest(const json &request) const
{
    return sendRequest(gameSocket, request, "game");
}

bool GameClient::authenticate()
{
    std::cout << "\n=== AUTHENTICATION PHASE ===" << std::endl;

    if (!connectToAuthServer())
    {
        return false;
    }

    std::cout << "\nEnter username: ";
    std::getline(std::cin, username);

    std::string password;
    std::cout << "Enter password: ";
    std::getline(std::cin, password);

    // Attempts to register the user in case they don't exist.
    std::cout << "\nAttempting to register user..." << std::endl;
    json registerReq;
    registerReq["action"] = "register";
    registerReq["username"] = username;
    registerReq["password"] = password;

    std::string response = sendAuthRequest(registerReq);
    std::cout << "Register response: " << response << std::endl;

    // Attempts to log in.
    std::cout << "\nAttempting to login..." << std::endl;
    json loginReq;
    loginReq["action"] = "login";
    loginReq["username"] = username;
    loginReq["password"] = password;

    response = sendAuthRequest(loginReq);
    std::cout << "Login response: " << response << std::endl;

    try
    {
        const json responseJson = json::parse(response);
        if (responseJson.value("success", false))
        {
            authToken = responseJson.value("token", "");
            if (!authToken.empty()) return true;
        }

        const std::string message = responseJson.value("message", "Unknown error");
        std::cout << "Login failed: " << message << std::endl;
    }
    catch (const json::parse_error& e)
    {
        std::cout << "Failed to parse login response: " << e.what() << std::endl;
    }

    return false;
}

void GameClient::gameMode()
{
    bool isAdmin = username == "admin";

    if (isAdmin)
    {
        std::cout << "ADMIN MODE ACTIVATED" << std::endl;
        std::cout << "Available commands:" << std::endl;
        std::cout << ":: list_users                    - Show all users with details" << std::endl;
        std::cout << ":: modify_type <username> <type> - Change user type (Freemium, Bronze, Silver, Gold, Platinum)" << std::endl;
        std::cout << ":: remove_user <username>        - Remove user from system" << std::endl;
        std::cout << ":: help                          - Show this help menu" << std::endl;
        std::cout << ":: quit                          - Exit" << std::endl;
    }
    else
    {
        std::cout << "Available commands:" << std::endl;
        std::cout << ":: adventure       - Go on an adventure" << std::endl;
        std::cout << ":: store           - Store pending item in inventory" << std::endl;
        std::cout << ":: list_items      - Show inventory" << std::endl;
        std::cout << ":: space           - Show inventory space" << std::endl;
        std::cout << ":: list_users      - Show online users" << std::endl;
        std::cout << ":: remove <itemId> - Remove item from inventory" << std::endl;
        std::cout << ":: sell <itemId>   - Sell item from inventory" << std::endl;
        std::cout << ":: help            - Show this help menu" << std::endl;
        std::cout << ":: quit            - Exit game" << std::endl;
    }

    std::cout << "\nType commands and press Enter. Use 'help' to see commands again." << std::endl;

    std::string command;
    while (true)
    {
        std::cout << "\n[" << username << (isAdmin ? " - ADMIN" : "") << "]> ";
        std::getline(std::cin, command);

        command.erase(0, command.find_first_not_of(" \t"));
        command.erase(command.find_last_not_of(" \t") + 1);

        if (command.empty())
        {
            continue;
        }

        if (command == "quit" || command == "exit")
        {
            std::cout << "Goodbye!" << std::endl;
            break;
        }

        if (command == "help")
        {
            if (isAdmin)
            {
                std::cout << "Available admin commands:" << std::endl;
                std::cout << ":: list_users              - Show all users with details" << std::endl;
                std::cout << ":: modify_type <username> <type> - Change user type" << std::endl;
                std::cout << ":: remove_user <username>  - Remove user from system" << std::endl;
                std::cout << ":: help                    - Show this help" << std::endl;
                std::cout << ":: quit                    - Exit" << std::endl;
            }
            else
            {
                std::cout << "Available commands:" << std::endl;
                std::cout << ":: adventure       - Go on an adventure" << std::endl;
                std::cout << ":: store           - Store pending item in inventory" << std::endl;
                std::cout << ":: list_items      - Show inventory" << std::endl;
                std::cout << ":: space           - Show inventory space" << std::endl;
                std::cout << ":: list_users      - Show online users" << std::endl;
                std::cout << ":: remove <itemId> - Remove item from inventory" << std::endl;
                std::cout << ":: sell <itemId>   - Sell item from inventory" << std::endl;
                std::cout << ":: help            - Show this help" << std::endl;
                std::cout << ":: quit            - Exit game" << std::endl;
            }
            continue;
        }

        json request;
        request["username"] = username;
        request["token"] = authToken;

        if (isAdmin)
        {
            if (command == "list_users")
            {
                request["action"] = "list_users";
            }
            else if (command.substr(0, 11) == "modify_type" && command.length() > 12)
            {
                std::istringstream iss(command.substr(12));
                std::string targetUser, newType;
                iss >> targetUser >> newType;

                if (!targetUser.empty() && !newType.empty())
                {
                    request["action"] = "modify_type";
                    request["targetUser"] = targetUser;
                    request["newType"] = newType;
                }
                else
                {
                    std::cout << "Usage: modify_type <username> <type>" << std::endl;
                    std::cout << "   Valid types: Freemium, Bronze, Silver, Gold, Platinum" << std::endl;
                    continue;
                }
            }
            else if (command.substr(0, 11) == "remove_user" && command.length() > 12)
            {
                std::string targetUser = command.substr(12);
                request["action"] = "remove_user";
                request["targetUser"] = targetUser;
            }
            else
            {
                std::cout << "Unknown admin command: '" << command << "'. Type 'help' for available commands." << std::endl;
                continue;
            }
        }
        else
        {
            if (command == "adventure")
                {
                request["action"] = "adventure";
            }
            else if (command == "store")
            {
                request["action"] = "store";
            }
            else if (command == "list_items")
            {
                request["action"] = "list_items";
            }
            else if (command == "space")
            {
                request["action"] = "space";
            }
            else if (command == "list_users")
            {
                request["action"] = "list_users";
            }
            else if (command.substr(0, 6) == "remove" && command.length() > 7)
            {
                request["action"] = "remove";
                request["itemId"] = command.substr(7);
            }
            else if (command.substr(0, 4) == "sell" && command.length() > 5)
            {
                request["action"] = "sell";
                request["itemId"] = command.substr(5);
            }
            else
            {
                std::cout << "Unknown command: '" << command << "'. Type 'help' for available commands." << std::endl;
                continue;
            }
        }

        std::string response = sendGameRequest(request);

        try
        {
            json responseJson = json::parse(response);

            bool success = responseJson.value("success", false);
            std::string message = responseJson.value("message", "");

            if (success)
            {
                std::cout << message << std::endl;
            }
            else
            {
                std::cout << "" << message << std::endl;
            }

            if (responseJson.contains("data"))
            {
                json data = responseJson["data"];

                if (data.contains("type"))
                {
                    if (std::string type = data["type"]; type == "item")
                    {
                        std::cout << "   Found item! Use 'store' to add it to your inventory." << std::endl;
                        if (data.contains("item"))
                        {
                            json item = data["item"];
                            std::cout << "   Item: " << item.value("name", "Unknown")
                                     << " (Weight: " << item.value("weight", 0)
                                     << ", Value: $" << Utilities::formatMoney(item.value("value", 0.0f)) << ")" << std::endl;
                        }
                    }
                    else if (type == "money")
                    {
                        std::cout << "   Money earned: $" << Utilities::formatMoney(data.value("amount", 0.0f)) << std::endl;
                        std::cout << "   Total balance: $" << Utilities::formatMoney(data.value("total_balance", 0.0f)) << std::endl;
                    }
                }

                if (data.contains("used_space") && data.contains("max_space"))
                {
                    std::cout << "   Inventory: " << data["used_space"] << "/" << data["max_space"] << " space used" << std::endl;
                }

                if (data.contains("inventory") && data["inventory"].is_array())
                {
                    if (auto inventory = data["inventory"]; !inventory.empty())
                    {
                        std::cout << "   Inventory items:" << std::endl;
                        for (const auto& item : inventory) {
                            std::cout << "     - " << item.value("name", "Unknown")
                                     << " [ID: " << item.value("id", "N/A")
                                     << "] (Weight: " << item.value("weight", 0)
                                     << ", Value: $" << item.value("value", 0.0f) << ")" << std::endl;
                        }
                    }
                    else
                    {
                        std::cout << "   Inventory is empty" << std::endl;
                    }
                }

                if (data.contains("users") && data["users"].is_array())
                {
                    auto users = data["users"];
                    if (!isAdmin) std::cout << "   Online users (" << users.size() << "):" << std::endl;
                    else std::cout << "   All users (" << users.size() << "):" << std::endl;
                    for (const auto& user : users)
                    {
                        std::cout << "     - " << user.value("username", "Unknown") << std::endl;
                    }
                }
            }

        }
        catch (...)
        {
            std::cout << "Raw response: " << response << std::endl;
        }
    }
}

void GameClient::disconnect()
{
    if (authSocket != INVALID_SOCKET)
    {
        closesocket(authSocket);
        authSocket = INVALID_SOCKET;
        std::cout << "Disconnected from the auth server" << std::endl;
    }

    if (gameSocket != INVALID_SOCKET)
    {
        closesocket(gameSocket);
        gameSocket = INVALID_SOCKET;
        std::cout << "Disconnected from the game Server" << std::endl;
    }
}

void GameClient::run()
{
    std::cout << "========================================" << std::endl;
    std::cout << "             GAME CLIENT" << std::endl;
    std::cout << "========================================" << std::endl;

    if (!authenticate())
    {
        std::cout << "Authentication failed. Exiting" << std::endl;
        return;
    }

    std::cout << "\n=== CONNECTING TO THE GAME SERVER ===" << std::endl;
    if (!connectToGameServer())
    {
        std::cout << "Failed to connect to the game server. Exiting" << std::endl;
        return;
    }

    gameMode();

    std::cout << "\n========================================" << std::endl;
    std::cout << "           SESSION COMPLETE" << std::endl;
    std::cout << "========================================" << std::endl;
}
