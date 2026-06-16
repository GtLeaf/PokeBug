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
    drawList();
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

void MenuScene::drawList() {
    const char* descs[ITEM_COUNT] = {
        "Feed",
        "Wood",
        "Fight",
        "Settings",
        "Info",
        "Back",
    };

    static constexpr int CENTER_Y = Hal::DISPLAY_H / 2;
    static constexpr int SPACING = 42;
    static constexpr float LERP = 0.25f;

    float fs = PixelRenderer::getContentFontScale();
    LGFX_Sprite& canvas = Hal::ins().canvas();

    // 让 animSelected 平滑追上 selected
    float target = (float)selected;
    float diff = target - animSelected;
    if (fabsf(diff) < 0.05f) {
        animSelected = target;
    } else {
        animSelected += diff * LERP;
    }

    // 按距离中心远近排序，确保选中项最后画
    int order[ITEM_COUNT];
    for (int i = 0; i < ITEM_COUNT; i++) order[i] = i;
    for (int i = 0; i < ITEM_COUNT - 1; i++) {
        for (int j = i + 1; j < ITEM_COUNT; j++) {
            float di = fabsf((float)order[i] - animSelected);
            float dj = fabsf((float)order[j] - animSelected);
            if (dj < di) {
                int t = order[i]; order[i] = order[j]; order[j] = t;
            }
        }
    }

    static constexpr int BOX_W = 56;
    static constexpr int BOX_H = 32;
    int boxX = (int)(10 * fs);

    for (int k = 0; k < ITEM_COUNT; k++) {
        int i = order[k];
        float rawOffset = (float)i - animSelected;
        int y = CENTER_Y + (int)(rawOffset * SPACING);

        bool isSelected = (fabsf(rawOffset) < 0.5f);
        float relScale = isSelected ? 1.15f : 1.0f;
        uint16_t boxColor = isSelected ? PixelRenderer::YELLOW : PixelRenderer::GRAY;
        uint16_t descColor = isSelected ? PixelRenderer::WHITE : PixelRenderer::GRAY;

        // 左侧统一尺寸的 item 色块，选中时以左上角为锚点放大 1.15x
        int drawBoxW = (int)(BOX_W * relScale);
        int drawBoxH = (int)(BOX_H * relScale);
        PixelRenderer::fillRect(boxX, y - drawBoxH / 2, drawBoxW, drawBoxH, boxColor);

        // 右侧说明文字，与色块垂直中心对齐；未选中时半透明（灰色）
        canvas.setFont(&fonts::Font0);
        canvas.setTextColor(descColor);
        canvas.setTextSize(fs);
        int descX = (boxX + drawBoxW + (int)(10 * fs) + Hal::DISPLAY_W) / 2;
        canvas.setTextDatum(MC_DATUM);  // 以文字中心为对齐点
        canvas.drawString(descs[i], descX, y);
        canvas.setTextDatum(TL_DATUM);  // 恢复左上角对齐
    }
}

bool MenuScene::onButton(const ButtonEvent& ev) {
    if (ev.action == BtnAction::LONG_PRESS) {
        nextScene = SCENE_TERRARIUM;
        return true;
    }

    if (ev.action == BtnAction::PRESSED) {
        if (ev.btn == 1) {
            // B：下一个（循环），从末尾跳回首项时直接跳变
            selected++;
            if (selected >= ITEM_COUNT) {
                selected = 0;
                animSelected = 0.0f;
            }
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
        case BACK:
            nextScene = SCENE_TERRARIUM;
            break;
    }
}
