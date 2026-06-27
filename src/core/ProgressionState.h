#pragma once
#include <cstdint>
#include "../game/FoodType.h"

struct ProgressionState {
    uint32_t featureFlags = 0;
    uint16_t foodUnlockMask = (uint16_t)(1U << (uint8_t)FoodType::DROP);
    uint8_t toyBallCount = 0;
    uint8_t activeToyBallDurability = 0;
};
