#ifndef HASHUTILS_H
#define HASHUTILS_H

#include <string>

class HashUtils
{
public:

    // Hashes passwords using SHA-256.
    static std::string hashPassword(const std::string& password);

    // Compares password hashes.
    static bool verifyPassword(const std::string& password, const std::string& hash);
};

#endif //HASHUTILS_H
