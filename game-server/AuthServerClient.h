#ifndef AUTHSERVERCLIENT_H
#define AUTHSERVERCLIENT_H

#include <winsock2.h>
#include <string>
#include <nlohmann/json.hpp>

class AuthServerClient
{
public:

    ~AuthServerClient();

    bool connect();
    void disconnect();
    bool isConnectionValid();
    bool reconnect();

    bool checkGlobalConnectionLimit();

    std::string sendRequest(const std::string& jsonRequest);

private:

    SOCKET authSocket = INVALID_SOCKET;
    std::mutex authSocketMutex;
    std::string serverAddress = "127.0.0.1";
    int serverPort = 8080;
    bool isConnected = false;
};

#endif //AUTHSERVERCLIENT_H
