#pragma once
#include <Arduino.h>
#include <cstdint>

namespace FoodAssets {

static constexpr uint8_t FRAME_W = 32;
static constexpr uint8_t FRAME_H = 16;

struct RleFrame {
    uint16_t offset;
    uint16_t length;
};

extern const uint8_t SPRITE_COUNT;
extern const RleFrame SPRITE_FRAMES[] PROGMEM;
extern const uint16_t SPRITE_RLE[] PROGMEM;

}
