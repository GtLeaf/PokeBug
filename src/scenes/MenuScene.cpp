#include "MenuScene.h"
#include "../core/GameEngine.h"
#include "../core/SaveManager.h"
#include "../core/UiStrings.h"
#include "../hardware/Hal.h"
#include "../hardware/PixelRenderer.h"
#include "../game/FoodType.h"
#include "../assets/FoodAssets.h"

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

    if (mode != Mode::FOOD) {
        drawBattery();
    }
    if (mode == Mode::FOOD) {
        drawFoodLayout();
    } else {
        drawList();
    }
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
            if (mode == Mode::FOOD) {
                // 食物子菜单：A 将当前高亮项设为全局食物（左侧菱形标记跟随），Back 返回主菜单
                if (selected < FOOD_ITEM_COUNT - 1) {
                    GameEngine::ins().setFoodStyle((uint8_t)selected);
                    foodConfirmTime = GameEngine::ins().getGameNow() + FOOD_CONFIRM_MS;
                    Serial.printf("[Menu] Global food set to: %s\n", FoodTypeInfo::name((FoodType)selected));
                } else {
                    enterMode(Mode::MAIN);
                }
            } else {
                executeSelection();
            }
            return true;
        }
    }

    return false;
}

void MenuScene::executeSelection() {
    Bug& bug = GameEngine::ins().getBug();
    if (mode == Mode::FOOD) {
        // 食物子菜单的放置/返回已在 onButton 中直接处理
        if (selected == FOOD_BACK) {
            enterMode(Mode::MAIN);
        }
        return;
    }

    if (mode == Mode::BOWL) {
        switch (selected) {
            case BOWL_STYLE:
                GameEngine::ins().cycleBowlStyle();
                bug.setFoodTray(GameEngine::ins().getBowlStyle() + 1,
                                (FoodType)GameEngine::ins().getFoodStyle());
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
                bug.setWood(GameEngine::ins().getWoodStyle());
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
    if (mode == Mode::FOOD) {
        // 默认定位到当前选中的食物（与培养缸短按 A 保持一致）
        selected = GameEngine::ins().getFoodStyle();
        if (selected >= FOOD_ITEM_COUNT - 1) selected = 0;
        foodScroll = 0.0f;       // 让列表在首帧自然滚动到选中项
        foodConfirmTime = 0;     // 清除上一次确认反馈
    }
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
        if (index >= 0 && index < FOOD_ITEM_COUNT - 1) {
            return FoodTypeInfo::name((FoodType)index);
        }
        return UiStrings::BACK;
    }

    if (mode == Mode::BOWL) {
        switch (index) {
            case BOWL_STYLE:
                snprintf(buf, bufSize, "%s:%s", UiStrings::TYPE,
                         GameEngine::ins().getBowlStyleName());
                return buf;
            case BOWL_BACK:
            default:
                return UiStrings::BACK;
        }
    }

    if (mode == Mode::WOOD) {
        switch (index) {
            case WOOD_STYLE:
                snprintf(buf, bufSize, "%s:%s", UiStrings::TYPE,
                         GameEngine::ins().getWoodStyleName());
                return buf;
            case WOOD_PLACE:
                return UiStrings::PLACE;
            case WOOD_BACK:
            default:
                return UiStrings::BACK;
        }
    }

    if (mode == Mode::BOX) {
        switch (index) {
            case BOX_WOOD:
                return UiStrings::MENU_WOOD;
            case BOX_BOWL:
                return UiStrings::MENU_BOWL;
            case BOX_BG:
                snprintf(buf, bufSize, "%s:%s", UiStrings::BG,
                         GameEngine::ins().getMainSceneBgName());
                return buf;
            case BOX_BACK:
            default:
                return UiStrings::BACK;
        }
    }

    switch (index) {
        case INFO: return UiStrings::MENU_INFO;
        case FEED: return UiStrings::MENU_FEED;
        case BOX: return UiStrings::MENU_BOX;
        case FIGHT: return UiStrings::MENU_FIGHT;
        case SETTINGS: return UiStrings::MENU_SETTINGS;
        case BACK:
        default:
            return UiStrings::BACK;
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



void MenuScene::drawFoodLayout() {
    Bug& bug = GameEngine::ins().getBug();
    LGFX_Sprite& canvas = Hal::ins().canvas();
    float fs = PixelRenderer::getContentFontScale();

    static constexpr int LEFT_W = 78;
    static constexpr int RIGHT_X = LEFT_W + 8;
    static constexpr int LIST_Y_START = 22;
    static constexpr int LIST_BOTTOM = Hal::DISPLAY_H - 8;
    static constexpr int VISIBLE_H = LIST_BOTTOM - LIST_Y_START;
    static constexpr int ROW_H = 22;          // 行高（含空隙），方便后续扩展更多食物
    static constexpr int TEXT_X = 12;
    static constexpr int MARK_X_OFS = 6;      // 文本与选择标记之间的距离

    // ---- 左侧可滚动列表 ----
    int totalH = FOOD_ITEM_COUNT * ROW_H;
    int maxScroll = (totalH > VISIBLE_H) ? (totalH - VISIBLE_H) : 0;
    int targetScroll = selected * ROW_H - VISIBLE_H / 2 + ROW_H / 2;
    if (targetScroll < 0) targetScroll = 0;
    if (targetScroll > maxScroll) targetScroll = maxScroll;

    // 平滑滚动
    float diff = (float)targetScroll - foodScroll;
    if (fabsf(diff) < 0.5f) {
        foodScroll = (float)targetScroll;
    } else {
        foodScroll += diff * 0.25f;
    }

    // 左右分隔线
    canvas.drawFastVLine(LEFT_W, 20, Hal::DISPLAY_H - 24, PixelRenderer::GRAY);

    // 绘制每个 item，仅绘制可见区域内的行
    for (int i = 0; i < FOOD_ITEM_COUNT; i++) {
        int y = LIST_Y_START + i * ROW_H - (int)foodScroll;
        if (y + ROW_H < LIST_Y_START || y > LIST_BOTTOM) continue;

        bool isSelected = (i == selected);
        uint16_t color = isSelected ? PixelRenderer::YELLOW : PixelRenderer::WHITE;

        const char* label;
        if (i == FOOD_ITEM_COUNT - 1) {
            label = UiStrings::BACK;
        } else {
            label = FoodTypeInfo::name((FoodType)i);
        }

        int textY = y + (ROW_H - (int)(8 * fs)) / 2;

        // 全局食物标记：在当前设置的食物名字前面绘制一个青色小菱形
        int globalFood = GameEngine::ins().getFoodStyle();
        if (i < FOOD_ITEM_COUNT - 1 && i == globalFood) {
            int cx = TEXT_X - 8;
            int cy = textY + (int)(4 * fs);
            uint16_t diamondColor = PixelRenderer::CYAN;
            // 确认反馈期间菱形也短暂变绿
            if (foodConfirmTime > 0 && GameEngine::ins().getGameNow() < foodConfirmTime && isSelected) {
                diamondColor = PixelRenderer::GREEN;
            }
            canvas.fillTriangle(cx, cy - 3, cx + 3, cy, cx, cy + 3, diamondColor);
            canvas.fillTriangle(cx, cy - 3, cx, cy + 3, cx - 3, cy, diamondColor);
        }

        PixelRenderer::drawPixelText(TEXT_X, textY, label, color, fs);
    }

    // ---- 右侧详情面板 ----
    if (selected >= 0 && selected < FOOD_ITEM_COUNT - 1) {
        FoodType ft = (FoodType)selected;
        int idx = selected;

        // 食物 icon：3x 放大，居中于右侧面板，位置上移给底部信息留空间
        uint16_t foodOffset = pgm_read_word(&FoodAssets::SPRITE_FRAMES[idx].offset);
        uint16_t foodLength = pgm_read_word(&FoodAssets::SPRITE_FRAMES[idx].length);
        int iconW = FoodAssets::FRAME_W * 3;
        int iconH = FoodAssets::FRAME_H * 3;
        int iconX = RIGHT_X + (Hal::DISPLAY_W - RIGHT_X - iconW) / 2;
        int iconY = 14;
        PixelRenderer::drawRgb565RleScaled(iconX, iconY,
                                           FoodAssets::FRAME_W,
                                           FoodAssets::FRAME_H,
                                           FoodAssets::SPRITE_RLE,
                                           foodOffset, foodLength, 3, false);

        // 名字 + 状态提示
        const char* name = FoodTypeInfo::name(ft);
        canvas.setTextSize(fs);
        int nameW = canvas.textWidth(name);
        int rightW = Hal::DISPLAY_W - RIGHT_X;
        int nameX = RIGHT_X + (rightW - nameW) / 2;
        int nameY = iconY + iconH + 8;
        PixelRenderer::drawPixelText(nameX, nameY, name, PixelRenderer::WHITE, fs);

        uint8_t count = bug.getFoodCount(ft);
        uint8_t level = FoodTypeInfo::level(ft);
        uint8_t trayLevel = GameEngine::ins().getBowlStyle() + 1;
        uint16_t hintColor = PixelRenderer::GREEN;
        const char* hint = ".";
        if (count == 0) {
            hintColor = PixelRenderer::RED;
            hint = "!";
        } else if (level > trayLevel) {
            hintColor = PixelRenderer::YELLOW;
            hint = "!";
        }
        PixelRenderer::drawPixelText(nameX + nameW + (int)(4 * fs),
                                     nameY,
                                     hint, hintColor, fs);

        // 食物信息：左对齐排列在右侧面板底部，预留更宽松的垂直空间
        static const char* attrNames[5] = {"SIZ", "STR", "END", "SPD", "SPI"};
        int attr = FoodTypeInfo::envAttribute(ft);
        const char* attrName = (attr >= 0 && attr < 5) ? attrNames[attr] : "-";

        char buf[48];
        int infoX = RIGHT_X + 10;
        int infoY = nameY + (int)(16 * fs);
        int infoStep = (int)(11 * fs);

        snprintf(buf, sizeof(buf), "Lv.%d", FoodTypeInfo::level(ft));
        PixelRenderer::drawPixelText(infoX, infoY, buf, PixelRenderer::CYAN, fs);
        infoY += infoStep;

        snprintf(buf, sizeof(buf), "%s: %s", UiStrings::TYPE, attrName);
        PixelRenderer::drawPixelText(infoX, infoY, buf, PixelRenderer::CYAN, fs);
        infoY += infoStep;

        snprintf(buf, sizeof(buf), "Stock: %d", count);
        PixelRenderer::drawPixelText(infoX, infoY, buf, PixelRenderer::CYAN, fs);
        infoY += infoStep;

        if (level > trayLevel) {
            snprintf(buf, sizeof(buf), "Need Lv.%d tray", level);
            PixelRenderer::drawPixelText(infoX, infoY, buf, PixelRenderer::RED, fs);
        } else if (count == 0) {
            PixelRenderer::drawPixelText(infoX, infoY, "Out of stock", PixelRenderer::RED, fs);
        } else {
            PixelRenderer::drawPixelText(infoX, infoY, "Ready", PixelRenderer::GREEN, fs);
        }
    } else if (selected == FOOD_ITEM_COUNT - 1) {
        // Back 选中时的右侧提示
        PixelRenderer::drawPixelText(RIGHT_X + 20, 80, "A:Back to menu",
                                     PixelRenderer::GRAY, fs);
    }
}
