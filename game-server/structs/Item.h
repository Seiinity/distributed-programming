#ifndef ITEM_H
#define ITEM_H

#include <string>
#include <utility>
#include <rpc.h>

struct Item
{
    std::string type;
    std::string name;
    int weight;
    float value;

    Item() : type(""), name(""), weight(0), value(0.0f) {}

    Item(const std::string& type, const std::string& name, const int weight, const float value)
    {
        this->type = type;
        this->name = name;
        this->weight = weight;
        this->value = value;
    }
};

struct ItemInstance
{
    GUID id{};
    Item item;

    ItemInstance()
    {
        UuidCreate(&this->id);
    }

    explicit ItemInstance(Item item): item(std::move(item))
    {
        UuidCreate(&this->id);
    }

    explicit ItemInstance(Item item, GUID id) : item(std::move(item)), id(id) { }
};

#endif //ITEM_H
