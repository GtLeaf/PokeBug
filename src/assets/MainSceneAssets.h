#pragma once
#include <Arduino.h>
#include <cstdint>

namespace MainSceneAssets {

extern const int MOSS_BG_W;
extern const int MOSS_BG_H;
extern const uint8_t MOSS_BG_INDEX[] PROGMEM;
extern const uint16_t MOSS_BG_PALETTE[] PROGMEM;

extern const int MOSS_GROUND_W;
extern const int MOSS_GROUND_H;
extern const uint8_t MOSS_GROUND_INDEX[] PROGMEM;
extern const uint16_t MOSS_GROUND_PALETTE[] PROGMEM;

extern const int MOSS_STATE_W;
extern const int MOSS_STATE_H;
extern const uint8_t MOSS_STATE_INDEX[] PROGMEM;
extern const uint16_t MOSS_STATE_PALETTE[] PROGMEM;

extern const int BEGINNER_FULL_W;
extern const int BEGINNER_FULL_H;
extern const uint8_t BEGINNER_FULL_INDEX[] PROGMEM;
extern const uint16_t BEGINNER_FULL_PALETTE[] PROGMEM;

extern const int CHILD_ROOM_FULL_W;
extern const int CHILD_ROOM_FULL_H;
extern const uint8_t CHILD_ROOM_FULL_INDEX[] PROGMEM;
extern const uint16_t CHILD_ROOM_FULL_PALETTE[] PROGMEM;

}
