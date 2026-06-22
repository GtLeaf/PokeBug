#pragma once
#include <Arduino.h>
#include <cstdint>

namespace ExploreAssets {

static constexpr int FRAME_W = 240;
static constexpr int FRAME_H = 135;
static constexpr uint8_t PARK_BG_COUNT = 3;
static constexpr uint8_t RIVERSIDE_BG_COUNT = 2;
static constexpr uint8_t OLD_WOODS_BG_COUNT = 2;

extern const uint16_t PARK_MORNING[] PROGMEM;
extern const uint16_t PARK_AFTERNOON[] PROGMEM;
extern const uint16_t PARK_EVENING[] PROGMEM;
extern const uint16_t BACK_HILL_DAY[] PROGMEM;
extern const uint16_t RIVERSIDE_DAY[] PROGMEM;
extern const uint16_t RIVERSIDE_NIGHT[] PROGMEM;
extern const uint16_t OLD_WOODS_DAY[] PROGMEM;
extern const uint16_t OLD_WOODS_NIGHT[] PROGMEM;

const uint16_t* parkBackground(uint8_t timeOfDay);
const uint16_t* background(uint8_t location, uint8_t timeOfDay);

}
