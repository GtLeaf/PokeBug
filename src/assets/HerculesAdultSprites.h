#pragma once
#include <Arduino.h>
#include <cstdint>

namespace HerculesAdultSprites {

static constexpr uint8_t FRAME_W = 48;
static constexpr uint8_t FRAME_H = 36;

struct RleFrame {
    uint16_t offset;
    uint16_t length;
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

extern const uint8_t RESET_FRAME_COUNT;
extern const RleFrame RESET_FRAMES[] PROGMEM;
extern const uint16_t RESET_RLE[] PROGMEM;

}
