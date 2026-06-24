#pragma once
#include <Arduino.h>
#include <cstdint>

namespace HerculesLarvaSprites {

static constexpr uint8_t AGE_COUNT = 3;
static constexpr uint8_t IDLE_FRAME_COUNT = 3;
static constexpr uint8_t SLEEP_FRAME_COUNT = 3;
static constexpr uint8_t EAT_FRAME_COUNT = 4;
static constexpr uint8_t IDLE_FRAME_W = 91;
static constexpr uint8_t IDLE_FRAME_H = 62;
static constexpr uint8_t SLEEP_FRAME_W = 91;
static constexpr uint8_t SLEEP_FRAME_H = 62;
static constexpr uint8_t EAT_FRAME_W = 91;
static constexpr uint8_t EAT_FRAME_H = 62;

struct RleFrame {
    uint16_t offset;
    uint16_t length;
    uint8_t width;
    uint8_t height;
};

extern const RleFrame IDLE_FRAMES[] PROGMEM;
extern const uint16_t IDLE_RLE[] PROGMEM;
extern const RleFrame SLEEP_FRAMES[] PROGMEM;
extern const uint16_t SLEEP_RLE[] PROGMEM;
extern const RleFrame EAT_FRAMES[] PROGMEM;
extern const uint16_t EAT_RLE[] PROGMEM;

}
