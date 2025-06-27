#include "GUIDUtils.h"

GUID GUIDUtils::stringToGUID(const std::string& str)
{
    GUID guid = {};
    sscanf_s(str.c_str(),
             "%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
             &guid.Data1, &guid.Data2, &guid.Data3,
             &guid.Data4[0], &guid.Data4[1], &guid.Data4[2], &guid.Data4[3],
             &guid.Data4[4], &guid.Data4[5], &guid.Data4[6], &guid.Data4[7]);
    return guid;
}

std::string GUIDUtils::GUIDToString(const GUID& guid)
{
    char guidStr[40];
    sprintf_s(guidStr, sizeof(guidStr),
        "%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
        guid.Data1, guid.Data2, guid.Data3,
        guid.Data4[0], guid.Data4[1], guid.Data4[2],
        guid.Data4[3], guid.Data4[4], guid.Data4[5],
        guid.Data4[6], guid.Data4[7]);

    return guidStr;
}
