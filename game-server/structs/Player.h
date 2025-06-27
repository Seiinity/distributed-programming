#ifndef PLAYER_H
#define PLAYER_H

#include <optional>
#include <string>
#include <vector>

#include "Item.h"
#include "../enums/PlayerType.h"
#include "../utilities/GUIDUtils.h"

struct Player
{
    std::string username;
    std::string authToken;
    PlayerType type;
    float balance;
    std::vector<ItemInstance> inventory;
    bool isAdmin;

    Player() : type(PlayerType::Freemium), balance(0), isAdmin(false) {}

    int getMaxInventorySpace() const
    {
        return static_cast<int>(type);
    }

    int getUsedInventorySpace() const
    {
        int used = 0;
        for (const auto& itemInstance : inventory)
        {
            used += itemInstance.item.weight;
        }

        return used;
    }

    bool canAddItem(const Item& item) const
    {
        return getUsedInventorySpace() + item.weight <= getMaxInventorySpace();
    }

    void collectItem(const ItemInstance& itemInstance)
    {
        inventory.push_back(itemInstance);
    }

    bool dropItem(const ItemInstance& itemInstance)
    {
        const auto itemIt = std::ranges::find_if(
            inventory,
            [&itemInstance](const ItemInstance& instance) {
                return IsEqualGUID(instance.id, itemInstance.id);
            });

        if (itemIt != inventory.end())
        {
            inventory.erase(itemIt);
            return true;
        }

        return false;
    }

    std::optional<ItemInstance> getItemFromInventory(const std::string& itemId)
    {
        // Finds item by GUID in the player's inventory.
        GUID targetGuid = GUIDUtils::stringToGUID(itemId);
        const auto itemIt = std::ranges::find_if(
            inventory,
            [&targetGuid](const ItemInstance& instance) {
                return IsEqualGUID(instance.id, targetGuid);
            });

        if (itemIt == inventory.end()) { return std::nullopt; }
        return *itemIt;
    }
};

#endif //PLAYER_H
