#pragma once
#include <Arduino.h>
#include <cstdint>

namespace HerculesAdultSprites {

static constexpr uint16_t MAX_FRAME_W = 76;
static constexpr uint16_t MAX_FRAME_H = 52;
static constexpr uint8_t BASE_SCALE_PERCENT = 144;
static constexpr uint8_t RUNTIME_MAX_SCALE_PERCENT = 120;
static constexpr uint8_t TERRARIUM_EDGE_HALF_W = 46;
static constexpr uint8_t TERRARIUM_MIN_X = 46;
static constexpr uint8_t TERRARIUM_MAX_X = 194;

static constexpr uint16_t PALETTE_KEY = 0xF800;

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

extern const uint8_t SLEEP_GETDOWN_FRAME_COUNT;
extern const RleFrame SLEEP_GETDOWN_FRAMES[] PROGMEM;
extern const uint16_t SLEEP_GETDOWN_RLE[] PROGMEM;

extern const uint8_t SLEEP_BREATH_FRAME_COUNT;
extern const RleFrame SLEEP_BREATH_FRAMES[] PROGMEM;
extern const uint16_t SLEEP_BREATH_RLE[] PROGMEM;

extern const uint8_t TURN_FRAME_COUNT;
extern const RleFrame TURN_FRAMES[] PROGMEM;
extern const uint16_t TURN_RLE[] PROGMEM;

extern const uint8_t THREATEN_FRAME_COUNT;
extern const RleFrame THREATEN_FRAMES[] PROGMEM;
extern const uint16_t THREATEN_RLE[] PROGMEM;

extern const uint8_t ATTACK_DOWN_FRAME_COUNT;
extern const RleFrame ATTACK_DOWN_FRAMES[] PROGMEM;
extern const uint16_t ATTACK_DOWN_RLE[] PROGMEM;

extern const uint8_t ATTACK_UP_FRAME_COUNT;
extern const RleFrame ATTACK_UP_FRAMES[] PROGMEM;
extern const uint16_t ATTACK_UP_RLE[] PROGMEM;

extern const uint8_t RESET_FRAME_COUNT;
extern const RleFrame RESET_FRAMES[] PROGMEM;
extern const uint16_t RESET_RLE[] PROGMEM;


}
