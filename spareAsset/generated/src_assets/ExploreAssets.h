#pragma once
#include <Arduino.h>
#include <cstdint>

namespace ExploreAssets {

static constexpr int FRAME_W = 240;
static constexpr int FRAME_H = 135;
static constexpr uint8_t PARK_BG_COUNT = 3;
static constexpr uint8_t RIVERSIDE_BG_COUNT = 2;
static constexpr uint8_t OLD_WOODS_BG_COUNT = 2;

struct IndexedImage {
    const uint8_t* indices;
    const uint16_t* palette;
};

extern const uint8_t PARK_MORNING_INDEX[] PROGMEM;
extern const uint16_t PARK_MORNING_PALETTE[] PROGMEM;
extern const uint8_t PARK_AFTERNOON_INDEX[] PROGMEM;
extern const uint16_t PARK_AFTERNOON_PALETTE[] PROGMEM;
extern const uint8_t PARK_EVENING_INDEX[] PROGMEM;
extern const uint16_t PARK_EVENING_PALETTE[] PROGMEM;
extern const uint8_t BACK_HILL_DAY_INDEX[] PROGMEM;
extern const uint16_t BACK_HILL_DAY_PALETTE[] PROGMEM;
extern const uint8_t RIVERSIDE_DAY_INDEX[] PROGMEM;
extern const uint16_t RIVERSIDE_DAY_PALETTE[] PROGMEM;
extern const uint8_t RIVERSIDE_NIGHT_INDEX[] PROGMEM;
extern const uint16_t RIVERSIDE_NIGHT_PALETTE[] PROGMEM;
extern const uint8_t OLD_WOODS_DAY_INDEX[] PROGMEM;
extern const uint16_t OLD_WOODS_DAY_PALETTE[] PROGMEM;
extern const uint8_t OLD_WOODS_NIGHT_INDEX[] PROGMEM;
extern const uint16_t OLD_WOODS_NIGHT_PALETTE[] PROGMEM;

IndexedImage parkBackground(uint8_t timeOfDay);
IndexedImage background(uint8_t location, uint8_t timeOfDay);

}
