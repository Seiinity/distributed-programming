#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <vector>
#include <string>
#include <map>
#include <fstream>
#include <random>
#include <windows.h>
#include <nlohmann/json.hpp>

#include "Structs/JsonMessage.h"
#include "Structs/User.h"
#include "utilities/HashUtils.h"
#include "utilities/JsonHelper.h"

namespace
{
    const std::string USERS_FILE = "users.json";

    std::map<std::string, User> users;
    std::atomic serverRunning{ true };
    SOCKET listenSocket = INVALID_SOCKET;
    std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());

    // Shuts the server down gracefully.
    void performShutdown()
    {
        if (!serverRunning.exchange(false)) return; // Prevents multiple shutdowns.

        std::cout << "\nShutting down the authentication server..." << std::endl;

        const StatusResponse status = JsonHelper::saveUsersToFile(USERS_FILE, users);
        std::cout << status.message << std::endl;

        if (listenSocket != INVALID_SOCKET)
        {
            closesocket(listenSocket);
            listenSocket = INVALID_SOCKET;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        WSACleanup();
        std::cout << "Authentication server shutdown complete." << std::endl;
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
    void ensureAdminAccountExists()
    {
        const std::string adminUsername = "admin";
        const std::string adminPassword = "Admin123!";

        if (users.contains(adminUsername)) return;

        User adminUser;
        adminUser.username = adminUsername;
        adminUser.passwordHash = HashUtils::hashPassword(adminPassword);
        adminUser.type = UserType::Freemium;
        adminUser.energy = 0;
        adminUser.isAdmin = true;

        users[adminUsername] = adminUser;
    }

    // Verifies that passwords are valid.
    // They must be between 8 and 20 characters and contain, at least, one uppercase and one lowercase letter,
    // a digit, and a special character.
    bool isValidPassword(const std::string& password)
    {
        if (password.length() < 8 || password.length() > 20)
        {
            return false;
        }

        bool hasUpper = false, hasLower = false, hasDigit = false, hasSpecial = false;

        for (const char ch : password)
        {
            if (ch >= 'A' && ch <= 'Z') hasUpper = true;
            else if (ch >= 'a' && ch <= 'z') hasLower = true;
            else if (ch >= '0' && ch <= '9') hasDigit = true;
            else hasSpecial = true;
        }

        return hasUpper && hasLower && hasDigit && hasSpecial;
    }

    // Validates the user's auth token (very simplified!).
    bool validateToken(const std::string& token, const std::string& username)
    {
        const std::string expectedPrefix = "AUTH_" + username + "_";
        return token.substr(0, expectedPrefix.length()) == expectedPrefix;
    }

    // Handles the 'register' command.
    std::string handleRegister(const std::string& username, const std::string& password)
    {
        if (users.contains(username))
        {
            return JsonHelper::createResponse(false, "User already exists");
        }

        if (!isValidPassword(password))
        {
            return JsonHelper::createResponse(false, "Password must be 8-20 chars with upper, lower, digit, and special char");
        }

        User newUser;
        newUser.username = username;
        newUser.passwordHash = HashUtils::hashPassword(password);
        newUser.type = UserType::Freemium;
        newUser.energy = 100;

        users[username] = newUser;

        std::cout << "New user registered: " << username << std::endl;
        return JsonHelper::createResponse(true, "User registered successfully");
    }

    // Handles the 'login' command.
    std::string handleLogin(const std::string& username, const std::string& password)
    {
        const auto it = users.find(username);

        if (it == users.end())
        {
            return JsonHelper::createResponse(false, "User not found");
        }

        if (const User& user = it->second; !HashUtils::verifyPassword(password, user.passwordHash))
        {
            return JsonHelper::createResponse(false, "Invalid password");
        }

        // Generates the auth token.
        const std::string token = "AUTH_" + username + "_" + std::to_string(time(nullptr));

        const User& user = it->second;
        json responseData;
        responseData["max_connections"] = user.type;

        std::cout << "User logged in: " << username << " (Type: " << userTypeToString(user.type)
              << ", Max connections: " << static_cast<int>(user.type) << ")" << std::endl;
        return JsonHelper::createResponse(true, "Login successful", token);
    }

    // Handles the 'check_energy' command.
    std::string handleCheckEnergy(const std::string& username, const std::string& token)
    {
        if (!validateToken(token, username))
        {
            return JsonHelper::createResponse(false, "Invalid authentication token");
        }

        const auto it = users.find(username);
        if (it == users.end())
        {
            return JsonHelper::createResponse(false, "User not found");
        }

        User& user = it->second;

        std::uniform_int_distribution energyCost(1, 2);
        int cost = energyCost(rng);

        if (user.energy < cost)
        {
            json responseData;
            responseData["current_energy"] = user.energy;
            responseData["required_energy"] = cost;
            return JsonHelper::createResponse(false, "Insufficient energy", token, responseData);
        }

        user.energy -= cost;

        json responseData;
        responseData["energy_cost"] = cost;
        responseData["remaining_energy"] = user.energy;
        responseData["username"] = username;

        std::cout << "Energy deducted for " << username << ": -" << cost << " (remaining: " << user.energy << ")" << std::endl;

        return JsonHelper::createResponse(true, "Energy deducted successfully", token, responseData);
    }

    // Handles the 'get_user_info' command.
    std::string handleGetUserInfo(const std::string& username, const std::string& token)
    {
        if (!validateToken(token, username))
        {
            return JsonHelper::createResponse(false, "Invalid authentication token");
        }

        const auto it = users.find(username);
        if (it == users.end())
        {
            return JsonHelper::createResponse(false, "User not found");
        }

        const User& user = it->second;

        json responseData;
        responseData["username"] = user.username;
        responseData["type"] = userTypeToString(user.type);
        responseData["energy"] = user.energy;
        responseData["connection_limit"] = user.type;

        return JsonHelper::createResponse(true, "User info retrieved", token, responseData);
    }

    // Handles the 'remove_user' command.
    std::string handleRemoveUser(const std::string& username, const std::string& token, const std::string& targetUser)
    {
        if (!validateToken(token, username))
        {
            return JsonHelper::createResponse(false, "Invalid authentication token");
        }

        const auto it = users.find(username);
        if (it == users.end())
        {
            return JsonHelper::createResponse(false, "User not found");
        }

        if (!it->second.isAdmin)
        {
            return JsonHelper::createResponse(false, "Insufficient permissions. Admin access required");
        }

        if (targetUser == username)
        {
            return JsonHelper::createResponse(false, "You may not remove yourself");
        }

        const auto targetIt = users.find(targetUser);
        if (targetIt == users.end())
        {
            return JsonHelper::createResponse(false, "Target user not found");
        }

        users.erase(targetIt);

        json responseData;
        responseData["removed_user"] = targetUser;
        responseData["remaining_users"] = users.size();

        return JsonHelper::createResponse(true, "User removed successfully: " + targetUser, token, responseData);
    }

    // Handles the 'modify_type' command.
    std::string handleModifyType(const std::string& username, const std::string& token, const std::string& targetUser, const std::string& newType)
    {
        if (!validateToken(token, username))
        {
            return JsonHelper::createResponse(false, "Invalid authentication token");
        }

        const auto it = users.find(username);
        if (it == users.end())
        {
            return JsonHelper::createResponse(false, "Player not found");
        }

        if (!it->second.isAdmin)
        {
            return JsonHelper::createResponse(false, "Insufficient permissions. Admin access required");
        }

        if (targetUser == username)
        {
            return JsonHelper::createResponse(false, "You may not modify your type");
        }

        const auto targetIt = users.find(targetUser);
        if (targetIt == users.end())
        {
            return JsonHelper::createResponse(false, "Target user not found");
        }

        const std::optional<UserType> newUserTypeOpt = stringToUserType(newType);
        if (!newUserTypeOpt.has_value())
        {
            return JsonHelper::createResponse(false, "Invalid player type. Valid types: Freemium, Bronze, Silver, Gold, Platinum");
        }

        const UserType newUserType = newUserTypeOpt.value();
        targetIt->second.type = newUserType;

        json responseData;
        responseData["target_user"] = targetUser;
        responseData["new_type"] = userTypeToString(newUserType);

        return JsonHelper::createResponse(true, "User type modified successfully: " + targetUser + " -> " + newType, token, responseData);
    }

    // Handles client connections.
    void handleClient(const SOCKET clientSocket, const int clientId)
    {
        std::cout << "Client " << clientId << " connected" << std::endl;

        char buffer[1024];
        std::string messageBuffer;

        while (serverRunning) // Handles the client until they disconnect.
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

                    if (msg.action == "register")
                    {
                        response = handleRegister(msg.username, msg.password);
                    }
                    else if (msg.action == "login")
                    {
                        response = handleLogin(msg.username, msg.password);
                    }
                    else if (msg.action == "check_energy")
                    {
                        response = handleCheckEnergy(msg.username, msg.authToken);
                    }
                    else if (msg.action == "get_user_info")
                    {
                        response = handleGetUserInfo(msg.username, msg.authToken);
                    }
                    else if (msg.action == "remove_user")
                    {
                        response = handleRemoveUser(msg.username, msg.authToken, msg.targetUser);
                    }
                    else if (msg.action == "modify_type")
                    {
                        response = handleModifyType(msg.username, msg.authToken, msg.targetUser, msg.newType);
                    }
                    else
                    {
                        response = JsonHelper::createResponse(false, "Unknown action: " + msg.action);
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

    // Loads users from the file.
    std::cout << "Loading user data..." << std::endl;
    const StatusResponse status = JsonHelper::loadUsersFromFile(USERS_FILE, users);
    std::cout << status.message << "\n" << std::endl;

    ensureAdminAccountExists();

    // Initialises Winsock (version 2.2).
    WSADATA wsaData;
    if (const int result = WSAStartup(MAKEWORD(2, 2), &wsaData); result != 0)
    {
        std::cout << "WSAStartup failed: " << result << std::endl;
        ExitProcess(EXIT_FAILURE);
    }

    std::cout << "Winsock initialized successfully" << std::endl;

    // Creates a socket with the TCP protocol for listening.
    listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET)
    {
        std::cout << "Socket creation failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        ExitProcess(EXIT_FAILURE);
    }

    std::cout << "Socket created successfully" << std::endl;

    // Configures the server address and port.
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(8080);

    // Binds the listening socket to the configured address.
    if (const int bindResult = bind(listenSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)); bindResult == SOCKET_ERROR)
    {
        std::cout << "Bind failed: " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        ExitProcess(EXIT_FAILURE);
    }

    std::cout << "Socket bound to port 8080" << std::endl;

    // Starts listening for incoming connections.
    if (const int listenResult = listen(listenSocket, SOMAXCONN); listenResult == SOCKET_ERROR)
    {
        std::cout << "Listen failed: " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        ExitProcess(EXIT_FAILURE);
    }

    std::cout << "\n============================================" << std::endl;
    std::cout << "Authentication Server listening on port 8080" << std::endl;
    std::cout << "============================================\n" << std::endl;

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

    return 0;
}