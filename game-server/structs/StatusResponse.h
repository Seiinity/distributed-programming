#ifndef STATUSRESPONSE_H
#define STATUSRESPONSE_H

#include <string>

struct StatusResponse
{
    bool success;
    std::string message;

    StatusResponse(const bool success, const std::string& message)
    {
        this->success = success;
        this->message = message;
    }
};

#endif //STATUSRESPONSE_H
