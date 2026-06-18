#pragma once
#include <Arduino.h>
#include <cstdint>

namespace ActionAssets {

static constexpr uint8_t FINGER_MAX_FRAME_W = 39;
static constexpr uint8_t FINGER_MAX_FRAME_H = 29;

struct RleFrame {
    uint16_t offset;
    uint16_t length;
    uint8_t width;
    uint8_t height;
    uint8_t tipX;
    uint8_t tipY;
};

extern const uint8_t FINGER_FRAME_COUNT;
extern const RleFrame FINGER_FRAMES[] PROGMEM;
extern const uint16_t FINGER_RLE[] PROGMEM;

}
