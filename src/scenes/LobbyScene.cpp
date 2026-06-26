#include "LobbyScene.h"
#include "../core/GameEngine.h"
#include "../core/UiStrings.h"
#include "../hardware/Hal.h"
#include "../hardware/PixelRenderer.h"
#include "../game/ItemCatalog.h"
#include <cmath>
#include <cstring>

namespace {

uint8_t visitPaletteCode(Temperament temperament, float depth) {
    uint8_t t = (uint8_t)temperament;
    if (t > (uint8_t)Temperament::SPIRIT) t = (uint8_t)Temperament::SPIRIT;
    if (depth < 0.0f) depth = 0.0f;
    if (depth > 1.0f) depth = 1.0f;
    uint8_t bucket = (uint8_t)(depth * 4.0f);
    if (bucket > 3) bucket = 3;
    return (uint8_t)(0x80 | (bucket << 3) | t);
}

}


#include "lobby/LobbySceneFlow.inc"
#include "lobby/LobbySceneRenderInput.inc"
