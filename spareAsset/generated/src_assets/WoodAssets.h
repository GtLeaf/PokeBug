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
    "Twisted fibers,", "Layered and solid,", "Damp and calming,", "Light and porous,", "Hollow wood,"
};
static constexpr const char* DESC_LINE2[SPRITE_COUNT] = {
    "sharpen its", "encourages", "soothes the", "keeps it", "builds tough"
};
static constexpr const char* DESC_LINE3[SPRITE_COUNT] = {
    "grip.", "steady bulk.", "mind.", "swift.", "resilience."
};

struct RleFrame {
    uint16_t offset;
    uint16_t length;
};

extern const RleFrame SPRITE_FRAMES[] PROGMEM;
extern const uint16_t SPRITE_RLE[] PROGMEM;

}
