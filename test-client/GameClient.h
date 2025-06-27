#ifndef GAMECLIENT_H
#define GAMECLIENT_H

#include <winsock2.h>
#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class GameClient
{
public:

    ~GameClient();

    bool connectToAuthServer();
    bool connectToGameServer();

    [[nodiscard]] std::string sendAuthRequest(const json& request) const;
    [[nodiscard]] std::string sendGameRequest(const json& request) const;

    bool authenticate();
    void gameMode();
    void disconnect();
    void run();

private:

    SOCKET authSocket = INVALID_SOCKET;
    SOCKET gameSocket = INVALID_SOCKET;
    std::string authToken;
    std::string username;

    static std::string sendRequest(SOCKET socket, const json &request, const std::string &serverType);
};

#endif //GAMECLIENT_H
