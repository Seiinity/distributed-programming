#include "AuthServerClient.h"

#include <iostream>
#include <ws2tcpip.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

AuthServerClient::~AuthServerClient()
{
    disconnect();
}

bool AuthServerClient::connect()
{
    std::lock_guard lock(authSocketMutex);

    if (isConnected)
    {
        return true;
    }

    authSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (authSocket == INVALID_SOCKET)
    {
        std::cout << "Failed to create the auth client socket: " << WSAGetLastError() << std::endl;
        return false;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    inet_pton(AF_INET, serverAddress.c_str(), &serverAddr.sin_addr);

    if (::connect(authSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR)
    {
        std::cout << "Failed to connect to the authentication server: " << WSAGetLastError() << std::endl;
        closesocket(authSocket);
        authSocket = INVALID_SOCKET;
        return false;
    }

    isConnected = true;
    std::cout << "Connected to the authentication server\n" << std::endl;
    return true;
}

void AuthServerClient::disconnect()
{
    std::lock_guard lock(authSocketMutex);

    if (authSocket != INVALID_SOCKET)
    {
        closesocket(authSocket);
        authSocket = INVALID_SOCKET;
        isConnected = false;
        std::cout << "Disconnected from the authentication server" << std::endl;
    }
}

bool AuthServerClient::isConnectionValid()
{
    std::lock_guard lock(authSocketMutex);
    return isConnected && authSocket != INVALID_SOCKET;
}

bool AuthServerClient::reconnect()
{
    std::cout << "Attempting to reconnect to the authentication server..." << std::endl;
    disconnect();
    return connect();
}

bool AuthServerClient::checkGlobalConnectionLimit()
{
    json request;
    request["action"] = "ping";

    std::string response = sendRequest(request.dump());
    if (response.empty())
    {
        return false;
    }

    try
    {
        if (json responseJson = json::parse(response); responseJson.contains("data"))
        {
            const json data = responseJson["data"];
            const int totalConnections = data.value("total_connections", 0);
            return totalConnections < 101; // 101 because this client counts as a connection and should be ignored.
        }
    }
    catch (...)
    {
        return false;
    }

    return true;
}

std::string AuthServerClient::sendRequest(const std::string& jsonRequest)
{
    std::lock_guard lock(authSocketMutex);

    if (!isConnected || authSocket == INVALID_SOCKET)
    {
        std::cout << "Not connected to auth server" << std::endl;
        return "";
    }

    if (send(authSocket, jsonRequest.c_str(), static_cast<int>(jsonRequest.length()), 0) == SOCKET_ERROR)
    {
        std::cout << "Failed to send to auth server: " << WSAGetLastError() << std::endl;
        isConnected = false;
        return "";
    }

    char buffer[1024];
    if (const int bytesReceived = recv(authSocket, buffer, sizeof(buffer) - 1, 0); bytesReceived > 0)
    {
        buffer[bytesReceived] = '\0';
        return { buffer };
    }
    else if (bytesReceived == 0)
    {
        std::cout << "The authentication server closed the connection" << std::endl;
        isConnected = false;
    }
    else
    {
        std::cout << "Failed to receive from the authentication server: " << WSAGetLastError() << std::endl;
        isConnected = false;
    }

    return "";
}
