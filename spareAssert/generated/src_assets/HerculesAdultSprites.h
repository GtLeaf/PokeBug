#pragma once
#include <Arduino.h>
#include <cstdint>

namespace HerculesAdultSprites {

static constexpr uint8_t MAX_FRAME_W = 58;
static constexpr uint8_t MAX_FRAME_H = 32;

struct RleFrame {
    uint16_t offset;
    uint16_t length;
    uint8_t width;
    uint8_t height;
};

extern const uint8_t WALK_FRAME_COUNT;
extern const RleFrame WALK_FRAMES[] PROGMEM;
extern const uint16_t WALK_RLE[] PROGMEM;

extern const uint8_t EAT_FRAME_COUNT;
extern const RleFrame EAT_FRAMES[] PROGMEM;
extern const uint16_t EAT_RLE[] PROGMEM;

extern const uint8_t TURN_FRAME_COUNT;
extern const RleFrame TURN_FRAMES[] PROGMEM;
extern const uint16_t TURN_RLE[] PROGMEM;

extern const uint8_t THREATEN_FRAME_COUNT;
extern const RleFrame THREATEN_FRAMES[] PROGMEM;
extern const uint16_t THREATEN_RLE[] PROGMEM;

extern const uint8_t RESET_FRAME_COUNT;
extern const RleFrame RESET_FRAMES[] PROGMEM;
extern const uint16_t RESET_RLE[] PROGMEM;

}
