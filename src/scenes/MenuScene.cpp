#include "MenuScene.h"
#include "../core/GameEngine.h"
#include "../core/SaveManager.h"
#include "../hardware/Hal.h"
#include "../hardware/PixelRenderer.h"

int MenuScene::lastSelected = 0;
int MenuScene::lastBoxSelected = 0;
int MenuScene::lastWoodSelected = 0;
int MenuScene::lastBowlSelected = 0;
int MenuScene::lastFoodSelected = 0;

void MenuScene::onEnter() {
    mode = Mode::MAIN;
    // 从培养缸进入时重置；从子菜单返回时保持上次位置
    if (GameEngine::ins().getPrevSceneID() == SCENE_TERRARIUM) {
        selected = 0;
    } else {
        selected = lastSelected;
    }
    animSelected = (float)selected;
}

void MenuScene::onExit() {
    if (mode == Mode::FOOD) {
        lastFoodSelected = selected;
    } else if (mode == Mode::BOWL) {
        lastBowlSelected = selected;
    } else if (mode == Mode::WOOD) {
        lastWoodSelected = selected;
    } else if (mode == Mode::BOX) {
        lastBoxSelected = selected;
    } else {
        lastSelected = selected;
    }
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
    float fs = PixelRenderer::getContentFontScale();
    int level = Hal::ins().batteryLevel();
    char buf[16];
    if (level < 0) {
        snprintf(buf, sizeof(buf), "--%%");
    } else {
        if (level > 100) level = 100;
        snprintf(buf, sizeof(buf), "%d%%", level);
    }
    uint16_t color = (level < 20) ? PixelRenderer::RED : PixelRenderer::GREEN;

    LGFX_Sprite& canvas = Hal::ins().canvas();
    canvas.setTextSize(fs);
    int w = canvas.textWidth(buf);
    int x = Hal::DISPLAY_W - w - (int)(6 * fs);
    PixelRenderer::drawPixelText(x, 6, buf, color, fs);
}

void MenuScene::drawList() {
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
    int count = itemCount();
    int order[MAIN_ITEM_COUNT];
    for (int i = 0; i < count; i++) order[i] = i;
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
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

    for (int k = 0; k < count; k++) {
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
        char label[24];
        const char* desc = itemLabel(i, label, sizeof(label));
        canvas.setTextSize(fs);
        int tw = canvas.textWidth(desc);
        int th = (int)(8 * fs);
        int descX = (boxX + drawBoxW + (int)(10 * fs) + Hal::DISPLAY_W) / 2;
        int descY = y - th / 2;
        PixelRenderer::drawPixelText(descX - tw / 2, descY, desc, descColor, fs);
    }
}

bool MenuScene::onButton(const ButtonEvent& ev) {
    if (ev.action == BtnAction::LONG_PRESS) {
        if (ev.btn == 0) {
            // 长按 A：返回主界面（培养缸）
            nextScene = SCENE_TERRARIUM;
            return true;
        }
        if (ev.btn == 1) {
            // 长按 B：返回上一级
            if (mode == Mode::FOOD) {
                enterMode(Mode::MAIN);
            } else if (mode == Mode::BOWL) {
                enterMode(Mode::BOX);
            } else if (mode == Mode::WOOD) {
                enterMode(Mode::BOX);
            } else if (mode == Mode::BOX) {
                enterMode(Mode::MAIN);
            } else {
                nextScene = SCENE_TERRARIUM;
            }
            return true;
        }
    }

    if (ev.action == BtnAction::PRESSED) {
        if (ev.btn == 1) {
            // B：下一个（循环），从末尾跳回首项时直接跳变
            selected++;
            if (selected >= itemCount()) {
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
    if (mode == Mode::FOOD) {
        switch (selected) {
            case FOOD_STYLE:
                GameEngine::ins().cycleFoodStyle();
                saveSettingsNow();
                Serial.printf("[Menu] Food style: %s\n", GameEngine::ins().getFoodStyleName());
                break;
            case FOOD_PLACE:
                if (bug.placeSapInTray()) {
                    Serial.printf("[Menu] Fed bug: %s\n", GameEngine::ins().getFoodStyleName());
                }
                nextScene = SCENE_TERRARIUM;
                break;
            case FOOD_BACK:
            default:
                enterMode(Mode::MAIN);
                break;
        }
        return;
    }

    if (mode == Mode::BOWL) {
        switch (selected) {
            case BOWL_STYLE:
                GameEngine::ins().cycleBowlStyle();
                saveSettingsNow();
                Serial.printf("[Menu] Bowl style: %s\n", GameEngine::ins().getBowlStyleName());
                break;
            case BOWL_BACK:
            default:
                enterMode(Mode::BOX);
                break;
        }
        return;
    }

    if (mode == Mode::WOOD) {
        switch (selected) {
            case WOOD_STYLE:
                GameEngine::ins().cycleWoodStyle();
                saveSettingsNow();
                Serial.printf("[Menu] Wood style: %s\n", GameEngine::ins().getWoodStyleName());
                break;
            case WOOD_PLACE:
                if (bug.placeWood()) {
                    Serial.printf("[Menu] Placed wood: %s\n", GameEngine::ins().getWoodStyleName());
                }
                nextScene = SCENE_TERRARIUM;
                break;
            case WOOD_BACK:
            default:
                enterMode(Mode::BOX);
                break;
        }
        return;
    }

    if (mode == Mode::BOX) {
        switch (selected) {
            case BOX_WOOD:
                enterMode(Mode::WOOD);
                break;
            case BOX_BOWL:
                enterMode(Mode::BOWL);
                break;
            case BOX_BG:
                GameEngine::ins().cycleMainSceneBg();
                saveSettingsNow();
                Serial.printf("[Menu] Main scene bg: %s\n", GameEngine::ins().getMainSceneBgName());
                break;
            case BOX_BACK:
                enterMode(Mode::MAIN);
                break;
        }
        return;
    }

    switch (selected) {
        case FEED:
            enterMode(Mode::FOOD);
            break;
        case BOX:
            enterMode(Mode::BOX);
            break;
        case FIGHT:
            nextScene = SCENE_LOBBY;
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

void MenuScene::enterMode(Mode nextMode) {
    if (mode == Mode::FOOD) lastFoodSelected = selected;
    else if (mode == Mode::BOWL) lastBowlSelected = selected;
    else if (mode == Mode::WOOD) lastWoodSelected = selected;
    else if (mode == Mode::BOX) lastBoxSelected = selected;
    else lastSelected = selected;

    mode = nextMode;
    if (mode == Mode::FOOD) selected = lastFoodSelected;
    else if (mode == Mode::BOWL) selected = lastBowlSelected;
    else if (mode == Mode::WOOD) selected = lastWoodSelected;
    else if (mode == Mode::BOX) selected = lastBoxSelected;
    else selected = lastSelected;
    int count = itemCount();
    if (selected >= count) selected = 0;
    animSelected = (float)selected;
}

int MenuScene::itemCount() const {
    if (mode == Mode::FOOD) return FOOD_ITEM_COUNT;
    if (mode == Mode::BOWL) return BOWL_ITEM_COUNT;
    if (mode == Mode::WOOD) return WOOD_ITEM_COUNT;
    if (mode == Mode::BOX) return BOX_ITEM_COUNT;
    return MAIN_ITEM_COUNT;
}

const char* MenuScene::itemLabel(int index, char* buf, size_t bufSize) const {
    if (mode == Mode::FOOD) {
        switch (index) {
            case FOOD_STYLE:
                snprintf(buf, bufSize, "Type:%s", GameEngine::ins().getFoodStyleName());
                return buf;
            case FOOD_PLACE:
                return "Place";
            case FOOD_BACK:
            default:
                return "Back";
        }
    }

    if (mode == Mode::BOWL) {
        switch (index) {
            case BOWL_STYLE:
                snprintf(buf, bufSize, "Type:%s", GameEngine::ins().getBowlStyleName());
                return buf;
            case BOWL_BACK:
            default:
                return "Back";
        }
    }

    if (mode == Mode::WOOD) {
        switch (index) {
            case WOOD_STYLE:
                snprintf(buf, bufSize, "Type:%s", GameEngine::ins().getWoodStyleName());
                return buf;
            case WOOD_PLACE:
                return "Place";
            case WOOD_BACK:
            default:
                return "Back";
        }
    }

    if (mode == Mode::BOX) {
        switch (index) {
            case BOX_WOOD:
                return "Wood";
            case BOX_BOWL:
                return "Bowl";
            case BOX_BG:
                snprintf(buf, bufSize, "BG:%s", GameEngine::ins().getMainSceneBgName());
                return buf;
            case BOX_BACK:
            default:
                return "Back";
        }
    }

    switch (index) {
        case INFO: return "Info";
        case FEED: return "Food";
        case BOX: return "Box";
        case FIGHT: return "Fight";
        case SETTINGS: return "Settings";
        case BACK:
        default:
            return "Back";
    }
}

void MenuScene::saveSettingsNow() {
    SaveManager::ins().saveSettings(
        PixelRenderer::getContentFontScale(),
        Hal::ins().getBrightness(),
        GameEngine::ins().getGameSpeed(),
        GameEngine::ins().getIdleTimeoutIndex(),
        GameEngine::ins().getMainSceneBg(),
        GameEngine::ins().getWoodStyle(),
        GameEngine::ins().getBowlStyle(),
        GameEngine::ins().getFoodStyle()
    );
}
