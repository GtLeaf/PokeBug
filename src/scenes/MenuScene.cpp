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
    animSelected = (float)selected;
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
    uint16_t color = (level < 20) ? PixelRenderer::RED : PixelRenderer::GREEN;
    PixelRenderer::drawPixelText(190, 6, buf, color, 1);
}

void MenuScene::drawCarousel() {
    const char* labels[ITEM_COUNT] = { "Feed", "Wood", "Fight", "Set", "Info" };
    static constexpr int CENTER_X = 120;
    static constexpr int CENTER_Y = 60;
    static constexpr int SPACING = 55;
    // 让 animSelected 以最短路径（循环）追上 selected，产生横向滚动补帧动画
    // 差值大时提高追及速度，避免快速连按时出现 1s 左右的延迟
    float target = (float)selected;
    float diff = target - animSelected;
    while (diff > ITEM_COUNT / 2.0f) diff -= ITEM_COUNT;
    while (diff < -ITEM_COUNT / 2.0f) diff += ITEM_COUNT;

    float absDiff = fabsf(diff);
    float speed = 0.35f;            // 单步切换的基础速度
    if (absDiff > 2.0f) speed = 0.85f;  // 跨越多项时快速追及
    else if (absDiff > 1.0f) speed = 0.6f;

    if (absDiff < 0.05f) {
        animSelected = target;
    } else {
        animSelected += diff * speed;
    }

    // 按距离中心远近排序绘制索引，确保选中项最后画在最上层
    int order[ITEM_COUNT];
    for (int i = 0; i < ITEM_COUNT; i++) order[i] = i;
    for (int i = 0; i < ITEM_COUNT - 1; i++) {
        for (int j = i + 1; j < ITEM_COUNT; j++) {
            float di = fabsf((float)order[i] - animSelected);
            if (di > ITEM_COUNT / 2.0f) di = ITEM_COUNT - di;
            float dj = fabsf((float)order[j] - animSelected);
            if (dj > ITEM_COUNT / 2.0f) dj = ITEM_COUNT - dj;
            if (dj < di) {
                int t = order[i]; order[i] = order[j]; order[j] = t;
            }
        }
    }

    for (int k = 0; k < ITEM_COUNT; k++) {
        int i = order[k];
        float rawOffset = (float)i - animSelected;
        if (rawOffset > ITEM_COUNT / 2.0f) rawOffset -= ITEM_COUNT;
        if (rawOffset < -ITEM_COUNT / 2.0f) rawOffset += ITEM_COUNT;

        int x = (int)(CENTER_X + rawOffset * SPACING);
        float dist = fabsf(rawOffset);
        float scale = 1.0f + 0.2f * (dist < 1.0f ? (1.0f - dist) : 0.0f);
        uint16_t boxColor = (dist < 0.5f) ? PixelRenderer::YELLOW : PixelRenderer::GRAY;
        uint16_t textColor = PixelRenderer::BLACK;

        int boxW = (int)(44 * scale);
        int boxH = (int)(36 * scale);
        PixelRenderer::fillRect(x - boxW / 2, CENTER_Y - boxH / 2, boxW, boxH, boxColor);

        LGFX_Sprite& canvas = Hal::ins().canvas();
        canvas.setTextSize(scale);
        int textW = canvas.textWidth(labels[i]);
        PixelRenderer::drawPixelText(x - textW / 2, CENTER_Y - 4, labels[i], textColor, scale);
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
