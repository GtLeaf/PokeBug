#include "MenuScene.h"
#include "../core/GameEngine.h"
#include "../hardware/Hal.h"
#include "../hardware/PixelRenderer.h"

int MenuScene::lastSelected = 0;

void MenuScene::onEnter() {
    // 从培养缸进入时重置；从子菜单返回时保持上次位置
    if (GameEngine::ins().getPrevSceneID() == SCENE_TERRARIUM) {
        selected = 0;
    } else {
        selected = lastSelected;
    }
}

void MenuScene::onExit() {
    lastSelected = selected;
}


SceneID MenuScene::update() {
    return nextScene;
}

void MenuScene::render() {
    PixelRenderer::fillRect(0, 0, 240, 135, PixelRenderer::rgb565(0, 0, 0));

    drawBattery();
    drawCarousel();

    PixelRenderer::drawPixelText(50, 110, "B:next  A:ok  long:back", PixelRenderer::WHITE, 1);
}

void MenuScene::drawBattery() {
    int level = Hal::ins().batteryLevel();
    char buf[16];
    if (level < 0) {
        snprintf(buf, sizeof(buf), "--%%");
    } else {
        if (level > 100) level = 100;
        snprintf(buf, sizeof(buf), "%d%%", level);
    }
    PixelRenderer::drawPixelText(200, 6, buf, PixelRenderer::WHITE, 1);

    // 电池外框
    PixelRenderer::fillRect(178, 4, 18, 10, PixelRenderer::WHITE);
    PixelRenderer::fillRect(180, 6, 14, 6, PixelRenderer::BLACK);
    PixelRenderer::fillRect(196, 6, 2, 6, PixelRenderer::WHITE);

    // 电量条
    uint16_t color = (level < 20) ? PixelRenderer::RED :
                     (level < 50) ? PixelRenderer::YELLOW : PixelRenderer::GREEN;
    int pct = (level < 0) ? 0 : level;
    int fillW = (14 * pct) / 100;
    if (fillW < 0) fillW = 0;
    if (fillW > 14) fillW = 14;
    PixelRenderer::fillRect(180, 6, fillW, 6, color);
}

void MenuScene::drawCarousel() {
    const char* labels[ITEM_COUNT] = { "Feed", "Wood", "Fight", "Set", "Info" };
    static constexpr int CENTER_X = 120;
    static constexpr int CENTER_Y = 60;
    static constexpr int SPACING = 55;

    // 按距离中心远近排序绘制索引
    int order[ITEM_COUNT];
    for (int i = 0; i < ITEM_COUNT; i++) order[i] = i;
    for (int i = 0; i < ITEM_COUNT - 1; i++) {
        for (int j = i + 1; j < ITEM_COUNT; j++) {
            int di = abs(order[i] - selected);
            if (di > ITEM_COUNT / 2) di = ITEM_COUNT - di;
            int dj = abs(order[j] - selected);
            if (dj > ITEM_COUNT / 2) dj = ITEM_COUNT - dj;
            if (dj < di) {
                int t = order[i]; order[i] = order[j]; order[j] = t;
            }
        }
    }

    for (int k = 0; k < ITEM_COUNT; k++) {
        int i = order[k];
        int rawOffset = i - selected;
        if (rawOffset > ITEM_COUNT / 2) rawOffset -= ITEM_COUNT;
        if (rawOffset < -ITEM_COUNT / 2) rawOffset += ITEM_COUNT;

        int x = CENTER_X + rawOffset * SPACING;
        bool isSelected = (i == selected);
        float scale = isSelected ? 1.2f : 1.0f;
        uint16_t boxColor = isSelected ? PixelRenderer::YELLOW : PixelRenderer::GRAY;
        uint16_t textColor = PixelRenderer::BLACK;

        int boxW = (int)(44 * scale);
        int boxH = (int)(36 * scale);
        PixelRenderer::fillRect(x - boxW / 2, CENTER_Y - boxH / 2, boxW, boxH, boxColor);
        PixelRenderer::drawPixelText(x - 16, CENTER_Y - 4, labels[i], textColor, scale);
    }
}

bool MenuScene::onButton(const ButtonEvent& ev) {
    if (ev.action == BtnAction::LONG_PRESS) {
        nextScene = SCENE_TERRARIUM;
        return true;
    }

    if (ev.action == BtnAction::PRESSED) {
        if (ev.btn == 1) {
            // B：下一个（循环）
            selected++;
            if (selected >= ITEM_COUNT) selected = 0;
            return true;
        }
        if (ev.btn == 0) {
            // A：确认
            executeSelection();
            return true;
        }
    }

    return false;
}

void MenuScene::executeSelection() {
    Bug& bug = GameEngine::ins().getBug();
    switch (selected) {
        case FEED:
            if (bug.placeSapInTray()) {
                Serial.println("[Menu] Fed bug");
            }
            nextScene = SCENE_TERRARIUM;
            break;
        case WOOD:
            if (bug.placeWood()) {
                Serial.println("[Menu] Placed wood");
            }
            nextScene = SCENE_TERRARIUM;
            break;
        case FIGHT:
            nextScene = SCENE_BATTLE;
            break;
        case SETTINGS:
            nextScene = SCENE_SETTINGS;
            break;
        case INFO:
            nextScene = SCENE_INFO;
            break;
    }
}
