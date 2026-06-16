#include "InfoScene.h"
#include "../core/GameEngine.h"
#include "../hardware/Hal.h"
#include "../hardware/PixelRenderer.h"

const char* InfoScene::STAGE_NAMES[4] = { "Egg", "Larva", "Pupa", "Adult" };

void InfoScene::onEnter() {}
void InfoScene::onExit() {}

SceneID InfoScene::update() {
    return nextScene;
}

void InfoScene::render() {
    Bug& bug = GameEngine::ins().getBug();

    PixelRenderer::fillRect(0, 0, 240, 135, PixelRenderer::BLACK);

    float fs = PixelRenderer::getContentFontScale();
    int lineH = (int)(13 * fs);
    int y = (int)(5 * fs);

    char buf[32];
    snprintf(buf, sizeof(buf), "Gen:%d %s", bug.getGeneration(), bug.isAlive() ? "alive" : "dead");
    PixelRenderer::drawPixelText(10, y, buf, PixelRenderer::WHITE);

    y += lineH;
    snprintf(buf, sizeof(buf), "Stage: %s", STAGE_NAMES[(int)bug.getStage()]);
    PixelRenderer::drawPixelText(10, y, buf, PixelRenderer::WHITE);

    // 属性条
    y += lineH + (int)(4 * fs);
    struct Attr { const char* name; float val; };
    Attr attrs[4] = {
        {"SIZ", bug.getSiz()},
        {"STR", bug.getStr()},
        {"END", bug.getEnd()},
        {"SPI", bug.getSpi()},
    };
    for (int i = 0; i < 4; i++) {
        PixelRenderer::drawPixelText(10, y, attrs[i].name, PixelRenderer::WHITE);
        PixelRenderer::drawProgressBar(45, y + 2, 100, 6, attrs[i].val / 10.0f, PixelRenderer::GREEN, PixelRenderer::GRAY);
        snprintf(buf, sizeof(buf), "%d", (int)roundf(attrs[i].val));
        PixelRenderer::drawPixelText(150, y, buf, PixelRenderer::WHITE);
        y += lineH;
    }

    snprintf(buf, sizeof(buf), "MOT:%d  HUN:%d", bug.getMot(), bug.getHunger());
    PixelRenderer::drawPixelText(10, y, buf, PixelRenderer::WHITE);

    y += lineH;
    snprintf(buf, sizeof(buf), "W/L: %d/%d  Sap:%d", bug.getWins(), bug.getLosses(), bug.getSap());
    PixelRenderer::drawPixelText(10, y, buf, PixelRenderer::WHITE);

    PixelRenderer::drawPixelText(10, Hal::DISPLAY_H - lineH, "Press A to return", PixelRenderer::GRAY);
}

bool InfoScene::onButton(const ButtonEvent& ev) {
    if (ev.action == BtnAction::LONG_PRESS) {
        nextScene = SCENE_TERRARIUM;
        return true;
    }
    if (ev.action == BtnAction::PRESSED && ev.btn == 0) {
        nextScene = SCENE_TERRARIUM;
        return true;
    }
    return false;
}
