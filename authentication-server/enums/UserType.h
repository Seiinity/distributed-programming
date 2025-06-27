#ifndef USERTYPE_H
#define USERTYPE_H

enum class UserType : std::uint8_t
{
    Freemium = 50,
    Bronze = 70,
    Silver = 90,
    Gold = 120,
    Platinum = 150
};

inline std::optional<UserType> stringToUserType(const std::string& string)
{
    if (string == "Freemium") return UserType::Freemium;
    if (string == "Bronze") return UserType::Bronze;
    if (string == "Silver") return UserType::Silver;
    if (string == "Gold") return UserType::Gold;
    if (string == "Platinum") return UserType::Platinum;
    return std::nullopt;
}

inline std::string userTypeToString(const UserType userType)
{
    switch (userType)
    {
        default:
        case UserType::Freemium:
            return "Freemium";
        case UserType::Bronze:
            return "Bronze";
        case UserType::Silver:
            return "Silver";
        case UserType::Gold:
            return "Gold";
        case UserType::Platinum:
            return "Platinum";
    }

}

#endif //USERTYPE_H
