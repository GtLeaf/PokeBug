#include "BattleScene.h"
#include "../core/GameEngine.h"
#include "../core/UiStrings.h"
#include "../hardware/Hal.h"
#include "../hardware/PixelRenderer.h"
#include "../game/BattleCalc.h"
#include "../assets/HerculesAdultSprites.h"

static uint8_t battleStatWithHunger(float value, uint8_t hunger) {
    int stat = (int)roundf(value);
    if (hunger < 10) stat -= 2;
    if (stat < 1) stat = 1;
    return (uint8_t)stat;
}

static uint16_t mixBattleRgb565(uint16_t base, uint16_t mix, float mixRatio) {
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

static uint16_t brightenBattleRgb565(uint16_t color, float factor) {
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

static uint16_t battleHueMain(Temperament temperament) {
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

static uint16_t battleDepthColor(Temperament temperament, float ratio) {
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    uint16_t base = battleHueMain(temperament);
    if (ratio < 0.25f) {
        return mixBattleRgb565(mixBattleRgb565(base, PixelRenderer::GRAY, 0.5f), PixelRenderer::WHITE, 0.3f);
    }
    if (ratio < 0.50f) {
        return mixBattleRgb565(base, PixelRenderer::GRAY, 0.3f);
    }
    if (ratio < 0.75f) {
        return base;
    }
    return brightenBattleRgb565(base, 1.25f);
}

static uint8_t battlePaletteCode(Temperament temperament, float depth) {
    uint8_t t = (uint8_t)temperament;
    if (t > (uint8_t)Temperament::SPIRIT) t = (uint8_t)Temperament::SPIRIT;
    if (depth < 0.0f) depth = 0.0f;
    if (depth > 1.0f) depth = 1.0f;
    uint8_t bucket = (uint8_t)(depth * 4.0f);
    if (bucket > 3) bucket = 3;
    return (uint8_t)(0x80 | (bucket << 3) | t);
}

static uint16_t battlePaletteColor(uint8_t palette) {
    if (palette & 0x80) {
        uint8_t t = palette & 0x07;
        if (t > (uint8_t)Temperament::SPIRIT) t = (uint8_t)Temperament::SPIRIT;
        uint8_t bucket = (palette >> 3) & 0x03;
        float depth = (bucket + 0.5f) / 4.0f;
        return battleDepthColor((Temperament)t, depth);
    }

    // 旧 0-3 调色板 fallback，仅用于旧存档/NPC 随机色。
    switch (palette & 0x03) {
        case 1: return 0x07E0;
        case 2: return 0xFE00;
        case 3: return 0xE71C;
        case 0:
        default:
            return 0xF800;
    }
}

static float rollNpcBattleSpiReward(bool win, NpcData::Tier tier) {
    if (!win) return 0.0f;

    uint8_t chance = 0;
    uint8_t minTenths = 0;
    uint8_t maxTenths = 0;
    switch (tier) {
        case NpcData::Tier::ROOKIE:
            chance = 30; minTenths = 1; maxTenths = 1;
            break;
        case NpcData::Tier::NORMAL:
            chance = 45; minTenths = 1; maxTenths = 2;
            break;
        case NpcData::Tier::VETERAN:
            chance = 60; minTenths = 2; maxTenths = 3;
            break;
        case NpcData::Tier::LEGEND:
            chance = 80; minTenths = 3; maxTenths = 4;
            break;
        default:
            return 0.0f;
    }

    if ((uint8_t)random(100) >= chance) return 0.0f;
    return (float)random(minTenths, maxTenths + 1) * 0.15f;
}


#include "battle/BattleSceneFlow.inc"
#include "battle/BattleSceneRounds.inc"
#include "battle/BattleSceneRender.inc"
