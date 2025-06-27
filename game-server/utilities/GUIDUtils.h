#ifndef GUIDUTILS_H
#define GUIDUTILS_H

#include <string>
#include <rpc.h>

class GUIDUtils
{
public:

    static GUID stringToGUID(const std::string& str);
    static std::string GUIDToString(const GUID& guid);
};

#endif //GUIDUTILS_H
