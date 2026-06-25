#pragma once
#include <Arduino.h>
#include <cstdint>

namespace WoodAssets {

static constexpr uint8_t FRAME_W = 105;
static constexpr uint8_t FRAME_H = 71;
static constexpr uint8_t SPRITE_COUNT = 5;

// 腐木风格名称与描述（暗示属性倾向，不直接展示增益）
static constexpr const char* NAME[SPRITE_COUNT] = {
    "Twig", "Stack", "Mossy", "Pale", "Hollow"
};
static constexpr const char* DESC_LINE1[SPRITE_COUNT] = {
    "Rough fibers.", "Solid layers.", "Cool moss.", "Dry wood.", "Deep hollow."
};
static constexpr const char* DESC_LINE2[SPRITE_COUNT] = {
    "Help claws", "Support body", "Calms mind", "Keeps legs", "Hardens shell"
};
static constexpr const char* DESC_LINE3[SPRITE_COUNT] = {
    "grip hard.", "and bulk.", "and spirit.", "quick.", "Deep endurance."
};

struct RleFrame {
    uint16_t offset;
    uint16_t length;
};

extern const RleFrame SPRITE_FRAMES[] PROGMEM;
extern const uint16_t SPRITE_RLE[] PROGMEM;

}
