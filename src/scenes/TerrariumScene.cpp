#include "TerrariumScene.h"
#include "../core/GameEngine.h"
#include "../core/UiStrings.h"
#include "../hardware/BattleLink.h"
#include "../hardware/Hal.h"
#include "../assets/MainSceneAssets.h"
#include "../assets/HerculesEggSprites.h"
#include "../assets/HerculesLarvaSprites.h"
#include "../assets/HerculesAdultSprites.h"
#include "../assets/HerculesPupaSprites.h"
#include "../assets/WoodAssets.h"
#include "../assets/BowlAssets.h"
#include "../assets/FoodAssets.h"
#include "../assets/SubstrateAssets.h"
#include "../assets/ActionAssets.h"
#include <cmath>
#include <cstring>

const uint16_t TerrariumScene::PALETTE[4][2] = {
    { PixelRenderer::BROWN, PixelRenderer::DARK_BROWN },
    { 0x0400, 0x0200 },   // 深绿/暗绿
    { 0xFE00, 0xA600 },   // 金色/暗金
    { 0xE71C, 0x8010 },   // 白化淡紫/暗紫
};

namespace {

constexpr int PUPA_BOTTOM_MARGIN_PX = 0;
constexpr int PUPA_POKE_SHAKE_PX = 3;
constexpr bool VISITOR_DEBUG_LOGS = false;
constexpr bool WOOD_ADULT_DEBUG_LOGS = false;
constexpr bool TOY_DEBUG_LOGS = true;
constexpr float TOY_BEETLE_HIT_HALF_W = 27.0f;
constexpr float TOY_BEETLE_HIT_TOP = 35.0f;
constexpr float TOY_BEETLE_HIT_BOTTOM = 5.0f;

bool isMobileBeetleStage(Stage stage) {
    return stage == Stage::ADULT || stage == Stage::JUVENILE;
}

bool circleHitsRect(float cx, float cy, float radius,
                    float left, float top, float right, float bottom) {
    float closestX = cx;
    if (closestX < left) closestX = left;
    if (closestX > right) closestX = right;
    float closestY = cy;
    if (closestY < top) closestY = top;
    if (closestY > bottom) closestY = bottom;

    float dx = cx - closestX;
    float dy = cy - closestY;
    return dx * dx + dy * dy <= radius * radius;
}

const char* adultStateName(AdultState state) {
    switch (state) {
        case AdultState::IDLE:        return "IDLE";
        case AdultState::WALK:        return "WALK";
        case AdultState::EAT:         return "EAT";
        case AdultState::TURN:        return "TURN";
        case AdultState::SLIDE:       return "SLIDE";
        case AdultState::CLIMB:       return "CLIMB";
        case AdultState::REST:        return "REST";
        case AdultState::THREATEN:    return "THREATEN";
        case AdultState::ATTACK_DOWN: return "ATTACK_DOWN";
        case AdultState::ATTACK_UP:   return "ATTACK_UP";
    }
    return "?";
}

uint16_t adultHueMain(Temperament temperament) {
    switch (temperament) {
        case Temperament::BRUTE:     return 0xF800; // 深红
        case Temperament::SWIFT:     return 0x6B7D; // 灰蓝
        case Temperament::GIANT:     return 0xFD20; // 橙褐
        case Temperament::RESILIENT: return 0xFE00; // 金色
        case Temperament::BALANCED:  return 0xFFFF; // 白/浅灰
        case Temperament::SPIRIT:    return 0x07E0; // 青绿
    }
    return 0xF800;
}

uint16_t mixRgb565(uint16_t base, uint16_t mix, float mixRatio) {
    if (mixRatio < 0.0f) mixRatio = 0.0f;
    if (mixRatio > 1.0f) mixRatio = 1.0f;
    uint8_t baseR = (base >> 11) & 0x1F;
    uint8_t baseG = (base >> 5) & 0x3F;
    uint8_t baseB = base & 0x1F;
    uint8_t mixR = (mix >> 11) & 0x1F;
    uint8_t mixG = (mix >> 5) & 0x3F;
    uint8_t mixB = mix & 0x1F;
    uint8_t r = (uint8_t)(baseR * (1.0f - mixRatio) + mixR * mixRatio);
    uint8_t g = (uint8_t)(baseG * (1.0f - mixRatio) + mixG * mixRatio);
    uint8_t b = (uint8_t)(baseB * (1.0f - mixRatio) + mixB * mixRatio);
    return (uint16_t)((r << 11) | (g << 5) | b);
}

uint16_t brightenRgb565(uint16_t color, float factor) {
    uint8_t r = (color >> 11) & 0x1F;
    uint8_t g = (color >> 5) & 0x3F;
    uint8_t b = color & 0x1F;
    uint16_t rr = (uint16_t)(r * factor);
    uint16_t gg = (uint16_t)(g * factor);
    uint16_t bb = (uint16_t)(b * factor);
    if (rr > 0x1F) rr = 0x1F;
    if (gg > 0x3F) gg = 0x3F;
    if (bb > 0x1F) bb = 0x1F;
    return (uint16_t)((rr << 11) | (gg << 5) | bb);
}

uint16_t adultDepthColor(Temperament temperament, float ratio) {
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    uint16_t base = adultHueMain(temperament);
    if (ratio < 0.25f) {
        return mixRgb565(mixRgb565(base, PixelRenderer::GRAY, 0.5f), PixelRenderer::WHITE, 0.3f);
    }
    if (ratio < 0.50f) {
        return mixRgb565(base, PixelRenderer::GRAY, 0.3f);
    }
    if (ratio < 0.75f) {
        return base;
    }
    return brightenRgb565(base, 1.25f);
}

uint16_t blendRgb565(uint16_t src, uint16_t dst, uint8_t alpha) {
    uint16_t inv = 255 - alpha;
    uint8_t sr = (uint8_t)(((src >> 11) & 0x1F) << 3);
    uint8_t sg = (uint8_t)(((src >> 5) & 0x3F) << 2);
    uint8_t sb = (uint8_t)((src & 0x1F) << 3);
    uint8_t dr = (uint8_t)(((dst >> 11) & 0x1F) << 3);
    uint8_t dg = (uint8_t)(((dst >> 5) & 0x3F) << 2);
    uint8_t db = (uint8_t)((dst & 0x1F) << 3);
    uint8_t r = (uint8_t)((sr * alpha + dr * inv) / 255);
    uint8_t g = (uint8_t)((sg * alpha + dg * inv) / 255);
    uint8_t b = (uint8_t)((sb * alpha + db * inv) / 255);
    return PixelRenderer::rgb565(r, g, b);
}

void drawRgb565RleFaded(int x, int y, int w, int h,
                        const uint16_t* data, uint16_t offset,
                        uint16_t length, uint8_t opacity) {
    if (!data || w <= 0 || h <= 0 || opacity == 0) return;
    LGFX_Sprite& canvas = Hal::ins().canvas();

    const uint16_t total = (uint16_t)(w * h);
    uint16_t idx = 0;
    uint16_t pixel = 0;
    while (idx < length && pixel < total) {
        uint16_t token = pgm_read_word(&data[offset + idx++]);
        uint16_t run = token & 0x7FFF;
        if (run == 0) continue;

        if (token & 0x8000) {
            pixel += run;
            if (pixel > total) pixel = total;
            continue;
        }

        for (uint16_t i = 0; i < run && idx < length && pixel < total; ++i, ++pixel) {
            uint16_t color = pgm_read_word(&data[offset + idx++]);
            int col = pixel % w;
            int row = pixel / w;
            int px = x + col;
            int py = y + row;
            if (px < 0 || py < 0 || px >= Hal::DISPLAY_W || py >= Hal::DISPLAY_H) continue;
            uint16_t bg = (uint16_t)canvas.readPixel(px, py);
            canvas.drawPixel(px, py, blendRgb565(color, bg, opacity));
        }
    }
}

void drawRgb565RleMappedScaledSheared(int x, int y, int w, int h,
                                      const uint16_t* data, uint16_t offset,
                                      uint16_t length, float scale,
                                      uint16_t keyMain, uint16_t targetMain,
                                      uint16_t keyShadow, uint16_t targetShadow,
                                      uint16_t keyMarking, uint16_t targetMarking,
                                      bool flipX, int topShearPx) {
    if (!data || w <= 0 || h <= 0 || scale <= 0.0f) return;
    LGFX_Sprite& canvas = Hal::ins().canvas();

    auto mapColor = [&](uint16_t color) -> uint16_t {
        if (color == keyMain) return targetMain;
        if (color == keyShadow) return targetShadow;
        if (color == keyMarking) return targetMarking;
        return color;
    };

    const uint16_t total = (uint16_t)(w * h);
    const int drawW = scale > 1.0f ? (int)ceilf(scale) : 1;
    const int drawH = scale > 1.0f ? (int)ceilf(scale) : 1;
    const int shearDen = h > 1 ? h - 1 : 1;
    uint16_t idx = 0;
    uint16_t pixel = 0;
    while (idx < length && pixel < total) {
        uint16_t token = pgm_read_word(&data[offset + idx++]);
        uint16_t run = token & 0x7FFF;
        if (run == 0) continue;

        if (token & 0x8000) {
            pixel += run;
            if (pixel > total) pixel = total;
            continue;
        }

        for (uint16_t i = 0; i < run && idx < length && pixel < total; ++i, ++pixel) {
            uint16_t color = mapColor(pgm_read_word(&data[offset + idx++]));
            int col = pixel % w;
            int row = pixel / w;
            if (flipX) col = w - 1 - col;

            // 伪倾斜：脚底行保持原位，越靠近背部/头顶越朝坡向错开。
            int shear = ((h - 1 - row) * topShearPx) / shearDen;
            int px = x + (int)(col * scale) + shear;
            int py = y + (int)(row * scale);
            canvas.fillRect(px, py, drawW, drawH, color);
        }
    }
}

}


#include "terrarium/TerrariumSceneCache.inc"
#include "terrarium/TerrariumSceneLifecycle.inc"
#include "terrarium/TerrariumSceneVisitor.inc"
#include "terrarium/TerrariumSceneToyCore.inc"
#include "terrarium/TerrariumSceneToyPhysics.inc"
#include "terrarium/TerrariumSceneSubstrate.inc"
#include "terrarium/TerrariumSceneLarvaState.inc"
#include "terrarium/TerrariumSceneUpdateInput.inc"
#include "terrarium/TerrariumSceneDrawCreatures.inc"
#include "terrarium/TerrariumSceneAdultMovement.inc"
#include "terrarium/TerrariumSceneHudTiltPoke.inc"
