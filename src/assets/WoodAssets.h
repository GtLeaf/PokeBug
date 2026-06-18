#pragma once
#include <Arduino.h>
#include <cstdint>

namespace WoodAssets {

static constexpr uint8_t FRAME_W = 88;
static constexpr uint8_t FRAME_H = 38;

struct RleFrame {
    uint16_t offset;
    uint16_t length;
};

extern const uint8_t WOOD_COUNT;
extern const RleFrame WOOD_FRAMES[] PROGMEM;
extern const uint16_t WOOD_RLE[] PROGMEM;

}
