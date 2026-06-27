#include "ExploreScene.h"
#include "../core/GameEngine.h"
#include "../core/UiStrings.h"
#include "../hardware/Hal.h"
#include "../hardware/PixelRenderer.h"
#include "../assets/ExploreAssets.h"
#include <cstring>

bool ExploreScene::sSessionActive = false;
uint8_t ExploreScene::sCurrentRound = 1;
int ExploreScene::sTotalSapGain = 0;
bool ExploreScene::sFinalRecorded = false;
bool ExploreScene::sReleaseEventSeen = false;

namespace {

static constexpr uint32_t EXPLORE_EVENT_DELAY_MS = 2000;
static constexpr uint32_t EXPLORE_REVEAL_SHAKE_MS = 360;
static constexpr int EXPLORE_REVEAL_SHAKE_PX = 2;

struct ExploreEventWeights {
    uint8_t sap;
    uint8_t food;
    uint8_t wood;
    uint8_t npc;
    uint8_t rare;
    uint8_t release;
    uint8_t nothing;
};

static constexpr ExploreEventWeights EVENT_TABLE[GameEngine::EXPLORE_LOCATION_COUNT][3] = {
    { // Park
        {40, 15, 10, 10, 5, 0, 20},
        {30, 20, 10, 15, 8, 0, 17},
        {25, 15, 10, 20, 10, 0, 20},
    },
    { // Back Hill
        {20, 10, 30, 10, 10, 8, 12},
        {30, 15, 20, 10, 10, 8, 7},
        {15, 10, 20, 25, 10, 8, 12},
    },
    { // Riverside
        {15, 15, 10, 15, 30, 0, 15},
        {25, 20, 10, 15, 15, 0, 15},
        {10, 10, 10, 20, 25, 0, 25},
    },
    { // Old Woods
        {10, 10, 15, 20, 25, 10, 10},
        {10, 10, 15, 25, 20, 10, 10},
        {5, 5, 10, 35, 20, 12, 13},
    },
};

static constexpr uint8_t NPC_TIER_TABLE[GameEngine::EXPLORE_LOCATION_COUNT][3][4] = {
    { // Park
        {70, 25, 5, 0},
        {70, 25, 5, 0},
        {70, 25, 5, 0},
    },
    { // Back Hill
        {40, 45, 15, 0},
        {40, 45, 14, 1},
        {35, 42, 20, 3},
    },
    { // Riverside
        {20, 40, 30, 10},
        {20, 40, 30, 10},
        {15, 35, 35, 15},
    },
    { // Old Woods
        {10, 30, 40, 20},
        {10, 30, 40, 20},
        {5, 25, 40, 30},
    },
};

uint8_t rgb565R(uint16_t color) {
    return (uint8_t)((color >> 8) & 0xF8);
}

uint8_t rgb565G(uint16_t color) {
    return (uint8_t)((color >> 3) & 0xFC);
}

uint8_t rgb565B(uint16_t color) {
    return (uint8_t)((color << 3) & 0xF8);
}

uint16_t blendRgb565(uint16_t dst, uint16_t src, uint8_t alpha) {
    uint8_t inv = 255 - alpha;
    uint8_t r = (uint8_t)((rgb565R(src) * alpha + rgb565R(dst) * inv) / 255);
    uint8_t g = (uint8_t)((rgb565G(src) * alpha + rgb565G(dst) * inv) / 255);
    uint8_t b = (uint8_t)((rgb565B(src) * alpha + rgb565B(dst) * inv) / 255);
    return PixelRenderer::rgb565(r, g, b);
}

void fillRectAlpha(int x, int y, int w, int h, uint16_t color, uint8_t alpha) {
    LGFX_Sprite& canvas = Hal::ins().canvas();
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > Hal::DISPLAY_W ? Hal::DISPLAY_W : x + w;
    int y1 = y + h > Hal::DISPLAY_H ? Hal::DISPLAY_H : y + h;
    for (int py = y0; py < y1; ++py) {
        for (int px = x0; px < x1; ++px) {
            canvas.drawPixel(px, py, blendRgb565(canvas.readPixel(px, py), color, alpha));
        }
    }
}

}


#include "explore/ExploreSceneFlow.inc"
#include "explore/ExploreSceneRender.inc"
