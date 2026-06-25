#pragma once
#include <Arduino.h>
#include <cstdint>
#include "FoodType.h"

using ItemId = uint16_t;

enum class ItemKind : uint8_t {
    NONE = 0,
    FOOD = 1,
    WOOD = 2,
};

struct ItemStack {
    ItemId id = 0;
    uint8_t amount = 0;
};

struct ItemInfo {
    ItemId id;
    ItemKind kind;
    const char* name;
    bool exchangeable;
    bool limited;
};

enum class WoodType : uint8_t {
    TWIG = 0,
    STACK,
    MOSSY,
    PALE,
    HOLLOW,
    COUNT,
};

namespace WoodTypeInfo {

static constexpr uint8_t COUNT = (uint8_t)WoodType::COUNT;

static constexpr const char* NAME[COUNT] = {
    "Twig", "Stack", "Mossy", "Pale", "Hollow"
};

// 0=SIZ, 1=STR, 2=END, 3=SPD, 4=SPI, -1=无
inline int envAttribute(WoodType type) {
    switch (type) {
        case WoodType::TWIG:   return 1; // STR
        case WoodType::STACK:  return 0; // SIZ
        case WoodType::MOSSY:  return 4; // SPI
        case WoodType::PALE:   return 3; // SPD
        case WoodType::HOLLOW: return 2; // END
        default: return -1;
    }
}

inline const char* name(WoodType type) {
    uint8_t idx = (uint8_t)type;
    return idx < COUNT ? NAME[idx] : NAME[0];
}

} // namespace WoodTypeInfo

namespace ItemCatalog {

static constexpr ItemId FOOD_BASE = 0x0100;
static constexpr ItemId WOOD_BASE = 0x0200;
static constexpr const char* UNKNOWN_NAME = "Unknown";

inline ItemId food(FoodType type) {
    return FOOD_BASE + (uint8_t)type;
}

inline ItemId wood(WoodType type) {
    return WOOD_BASE + (uint8_t)type;
}

inline ItemKind kind(ItemId id) {
    if (id >= FOOD_BASE && id < FOOD_BASE + (uint8_t)FoodType::COUNT) return ItemKind::FOOD;
    if (id >= WOOD_BASE && id < WOOD_BASE + WoodTypeInfo::COUNT) return ItemKind::WOOD;
    return ItemKind::NONE;
}

inline uint8_t index(ItemId id) {
    switch (kind(id)) {
        case ItemKind::FOOD: return (uint8_t)(id - FOOD_BASE);
        case ItemKind::WOOD: return (uint8_t)(id - WOOD_BASE);
        default: return 0;
    }
}

inline const char* name(ItemId id) {
    switch (kind(id)) {
        case ItemKind::FOOD: return FoodTypeInfo::name((FoodType)index(id));
        case ItemKind::WOOD: return WoodTypeInfo::name((WoodType)index(id));
        default: return UNKNOWN_NAME;
    }
}

inline bool isExchangeable(ItemId id) {
    return kind(id) == ItemKind::FOOD || kind(id) == ItemKind::WOOD;
}

inline bool isLimited(ItemId id) {
    (void)id;
    return false;
}

inline ItemInfo info(ItemId id) {
    return {id, kind(id), name(id), isExchangeable(id), isLimited(id)};
}

} // namespace ItemCatalog
