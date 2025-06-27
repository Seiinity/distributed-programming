#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>

#include "GameClient.h"

#pragma comment(lib, "Ws2_32.lib")

int main()
{
    // Initialises Winsock (version 2.2).
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cout << "WSAStartup failed" << std::endl;
        return 1;
    }

    GameClient client;
    client.run();

    // Cleanup.
    WSACleanup();

    std::cout << "\nPress Enter to exit..." << std::endl;
    std::cin.get();

    return 0;
}