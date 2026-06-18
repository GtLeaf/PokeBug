#pragma once
#include <Arduino.h>
#include <cstdint>

namespace ActionAssets {

static constexpr uint8_t FINGER_FRAME_W = 48;
static constexpr uint8_t FINGER_FRAME_H = 40;

struct RleFrame {
    uint16_t offset;
    uint16_t length;
};

extern const uint8_t FINGER_FRAME_COUNT;
extern const RleFrame FINGER_FRAMES[] PROGMEM;
extern const uint16_t FINGER_RLE[] PROGMEM;

}
