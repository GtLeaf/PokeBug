#pragma once
#include <Arduino.h>
#include <cstdint>

// 食物类型枚举与属性表
// 参考 doc/PokeBug-FoodSystem.md

enum class FoodType : uint8_t {
    DROP = 0,    // 水滴
    CUBE,        // 方块/坚果
    SLICE,       // 切片/香蕉
    CITRUS,      // 柑橘
    JELLY,       // 果冻/蜂蜜
    BERRY,       // 浆果
    COUNT
};

namespace FoodTypeInfo {

// 食物属性向量：SIZ, STR, END, SPD, SPI
struct FoodStats {
    float siz;
    float str;
    float end;
    float spd;
    float spi;
};

// 基础营养值（一次喂养）
static constexpr FoodStats STATS[(uint8_t)FoodType::COUNT] = {
    /* DROP   */ {0.30f, 0.05f, 0.00f, 0.00f, 0.00f},
    /* CUBE   */ {0.05f, 0.30f, 0.00f, 0.00f, 0.00f},
    /* SLICE  */ {0.15f, 0.15f, 0.00f, 0.15f, 0.00f},
    /* CITRUS */ {0.10f, 0.08f, 0.00f, 0.05f, 0.15f},
    /* JELLY  */ {0.20f, 0.25f, 0.00f, 0.20f, 0.00f},
    /* BERRY  */ {0.15f, 0.15f, 0.15f, 0.10f, 0.10f},
};

// 食物等级
static constexpr uint8_t LEVEL[(uint8_t)FoodType::COUNT] = {
    1, 1, 2, 2, 3, 3
};

// 食物名称
static constexpr const char* NAME[(uint8_t)FoodType::COUNT] = {
    "Drop", "Cube", "Slice", "Citrus", "Jelly", "Berry"
};

// 成虫期 MOT 恢复量
static constexpr uint8_t ADULT_MOT_RECOVERY[(uint8_t)FoodType::COUNT] = {
    10, 8, 20, 15, 30, 25
};

// 是否为 Jelly（间隔惩罚）
inline bool hasIntervalPenalty(FoodType t) {
    return t == FoodType::JELLY;
}

// Jelly 间隔惩罚阈值（ms）
static constexpr uint32_t JELLY_INTERVAL_MS = 5ULL * 60 * 1000;

// 食物对应属性倾向（环境加成用）
// 0=SIZ, 1=STR, 2=END, 3=SPD, 4=SPI, -1=无
inline int envAttribute(FoodType t) {
    switch (t) {
        case FoodType::DROP:   return 0; // SIZ
        case FoodType::CUBE:   return 1; // STR
        case FoodType::SLICE:  return 3; // SPD
        case FoodType::CITRUS: return 4; // SPI
        case FoodType::JELLY:  return 1; // STR
        case FoodType::BERRY:  return 2; // END
        default: return -1;
    }
}

inline const FoodStats& stats(FoodType t) {
    return STATS[(uint8_t)t];
}

inline const char* name(FoodType t) {
    return NAME[(uint8_t)t];
}

inline uint8_t level(FoodType t) {
    return LEVEL[(uint8_t)t];
}

inline uint8_t adultMotRecovery(FoodType t) {
    return ADULT_MOT_RECOVERY[(uint8_t)t];
}

} // namespace FoodTypeInfo
