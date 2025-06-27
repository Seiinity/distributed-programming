#ifndef CONNECTIONINFO_H
#define CONNECTIONINFO_H

#include <chrono>
#include <string>
#include <winsock2.h>

struct ConnectionInfo
{
    std::string username;
    int limit;
    std::chrono::steady_clock::time_point connectTime;
    SOCKET socket;
};

#endif //CONNECTIONINFO_H
