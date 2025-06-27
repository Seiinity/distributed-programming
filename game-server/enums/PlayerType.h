#ifndef PLAYERTYPE_H
#define PLAYERTYPE_H

enum class PlayerType
{
    Freemium = 20,
    Bronze = 40,
    Silver = 60,
    Gold = 80,
    Platinum = 120
};

inline std::optional<PlayerType> stringToPlayerType(const std::string& string)
{
    if (string == "Freemium") return PlayerType::Freemium;
    if (string == "Bronze") return PlayerType::Bronze;
    if (string == "Silver") return PlayerType::Silver;
    if (string == "Gold") return PlayerType::Gold;
    if (string == "Platinum") return PlayerType::Platinum;
    return std::nullopt;
}

inline std::string playerTypeToString(const PlayerType playerType)
{
    switch (playerType)
    {
        default:
        case PlayerType::Freemium:
            return "Freemium";
        case PlayerType::Bronze:
            return "Bronze";
        case PlayerType::Silver:
            return "Silver";
        case PlayerType::Gold:
            return "Gold";
        case PlayerType::Platinum:
            return "Platinum";
    }

}

#endif //PLAYERTYPE_H