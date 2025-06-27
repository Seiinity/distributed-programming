#include "HashUtils.h"

#include <windows.h>
#include <bemapiset.h>
#include <iomanip>
#include <ios>
#include <wincrypt.h>

std::string HashUtils::hashPassword(const std::string& password)
{
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    DWORD hashLen = 32;
    std::string result;

    if (CryptAcquireContext(&hProv, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
    {
        if (CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash))
        {
            if (CryptHashData(hHash, reinterpret_cast<const BYTE*>(password.c_str()), password.length(), 0))
            {
                BYTE hash[32];

                if (CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0))
                {
                    std::stringstream stream;
                    for (DWORD i = 0; i < hashLen; i++)
                    {
                        stream << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(hash[i]);
                    }

                    result = stream.str();
                }
            }

            CryptDestroyHash(hHash);
        }

        CryptReleaseContext(hProv, 0);
    }

    return result;
}

bool HashUtils::verifyPassword(const std::string& password, const std::string& hash)
{
    return hashPassword(password) == hash;
}
