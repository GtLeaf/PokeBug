#pragma once
#include <Arduino.h>
#include <cstdint>

namespace BowlAssets {

static constexpr uint8_t FRAME_W = 52;
static constexpr uint8_t FRAME_H = 27;
static constexpr uint8_t SPRITE_COUNT = 3;

// 食物盘风格名称与描述
static constexpr const char* NAME[SPRITE_COUNT] = {
    "Low", "Block", "Root"
};
static constexpr const char* DESC_LINE1[SPRITE_COUNT] = {
    "Shallow and easy,", "Raised edges,", "Carved like bark,"
};
static constexpr const char* DESC_LINE2[SPRITE_COUNT] = {
    "basic tray", "keeps food in,", "it blends well"
};
static constexpr const char* DESC_LINE3[SPRITE_COUNT] = {
    "for daily feed.", "no spills.", "with nature."
};

struct RleFrame {
    uint16_t offset;
    uint16_t length;
};

extern const RleFrame SPRITE_FRAMES[] PROGMEM;
extern const uint16_t SPRITE_RLE[] PROGMEM;

}
