#pragma once
#include <Arduino.h>
#include <cstdint>

namespace HerculesPupaSprites {

static constexpr uint8_t FRAME_COUNT = 3;
static constexpr uint8_t FRAME_W = 58;
static constexpr uint8_t FRAME_H = 70;

struct RleFrame {
    uint16_t offset;
    uint16_t length;
    uint8_t width;
    uint8_t height;
};

extern const RleFrame FRAMES[] PROGMEM;
extern const uint16_t RLE[] PROGMEM;

}
