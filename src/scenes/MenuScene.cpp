#include "MenuScene.h"
#include "../core/GameEngine.h"
#include "../core/SaveManager.h"
#include "../core/UiStrings.h"
#include "../hardware/Hal.h"
#include "../hardware/PixelRenderer.h"
#include "../game/FoodType.h"
#include "../assets/FoodAssets.h"
#include "../assets/WoodAssets.h"
#include "../assets/BowlAssets.h"
#include "../assets/MenuAssets.h"

int MenuScene::lastSelected = 0;
int MenuScene::lastBoxSelected = 0;
int MenuScene::lastWoodSelected = 0;
int MenuScene::lastBowlSelected = 0;
int MenuScene::lastFoodSelected = 0;
int MenuScene::lastFightSelected = 0;
int MenuScene::lastExploreSelected = 0;

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
    } else if (mode == Mode::FIGHT) {
        lastFightSelected = selected;
    } else if (mode == Mode::EXPLORE) {
        lastExploreSelected = selected;
    } else {
        lastSelected = selected;
    }
}


SceneID MenuScene::update() {
    return nextScene;
}

void MenuScene::render() {
    PixelRenderer::fillRect(0, 0, 240, 135, PixelRenderer::rgb565(0, 0, 0));

    if (mode == Mode::MAIN) {
        drawBattery();
    }
    if (mode == Mode::FOOD) {
        drawFoodLayout();
    } else if (mode == Mode::WOOD) {
        drawWoodLayout();
    } else if (mode == Mode::BOWL) {
        drawBowlLayout();
    } else if (mode == Mode::FIGHT) {
        drawFightList();
    } else if (mode == Mode::EXPLORE) {
        drawExploreList();
    } else {
        drawList();
    }

    drawToast();
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
    static constexpr int ICON_SLOT_W = 56;
    int boxX = (int)(10 * fs) + 10;

    for (int k = 0; k < count; k++) {
        int i = order[k];
        float rawOffset = (float)i - animSelected;
        int y = CENTER_Y + (int)(rawOffset * SPACING);

        bool isSelected = (fabsf(rawOffset) < 0.5f);
        bool cupAvailable = !(mode == Mode::FIGHT && i == FIGHT_CUP) || isCupAvailable();
        float relScale = isSelected ? 1.15f : 1.0f;
        uint16_t boxColor = (isSelected && cupAvailable) ? PixelRenderer::YELLOW : PixelRenderer::GRAY;
        uint16_t descColor = (isSelected && cupAvailable) ? PixelRenderer::WHITE : PixelRenderer::GRAY;

        int leftW = BOX_W;
        if (mode == Mode::MAIN && i < MenuAssets::MAIN_ICON_COUNT) {
            if (isSelected) {
                PixelRenderer::fillRect(boxX - 4, y - BOX_H / 2, 2, BOX_H, PixelRenderer::YELLOW);
            }

            uint16_t offset = pgm_read_word(&MenuAssets::MAIN_ICON_FRAMES[i].offset);
            uint16_t length = pgm_read_word(&MenuAssets::MAIN_ICON_FRAMES[i].length);
            int scaledIconW = (int)(MenuAssets::FRAME_W * relScale);
            int scaledIconH = (int)(MenuAssets::FRAME_H * relScale);
            int iconX = boxX + (ICON_SLOT_W - scaledIconW) / 2;
            int iconY = y - scaledIconH / 2;
            PixelRenderer::drawRgb565RleScaled(iconX, iconY, MenuAssets::FRAME_W, MenuAssets::FRAME_H,
                                               MenuAssets::MAIN_ICON_RLE, offset, length, relScale);
            leftW = ICON_SLOT_W;
        } else {
            // 子菜单仍使用统一尺寸色块；主菜单由 MenuAssets 图标承担视觉识别。
            int drawBoxW = (int)(BOX_W * relScale);
            int drawBoxH = (int)(BOX_H * relScale);
            PixelRenderer::fillRect(boxX, y - drawBoxH / 2, drawBoxW, drawBoxH, boxColor);
            leftW = drawBoxW;
        }

        // 右侧说明文字，与色块垂直中心对齐；未选中时半透明（灰色）
        char label[24];
        const char* desc = itemLabel(i, label, sizeof(label));
        canvas.setTextSize(fs);
        int tw = canvas.textWidth(desc);
        int th = (int)(8 * fs);
        int descX = (boxX + leftW + (int)(10 * fs) + Hal::DISPLAY_W) / 2 - 10;
        int descY = y - th / 2;
        PixelRenderer::drawPixelText(descX - tw / 2, descY, desc, descColor, fs);
    }
}

void MenuScene::drawFightList() {
    const char* items[FIGHT_ITEM_COUNT] = {
        UiStrings::MENU_FIGHT_CUP,
        UiStrings::MENU_FIGHT_CREATE,
        UiStrings::MENU_FIGHT_SEARCH,
        UiStrings::BACK,
    };

    float fs = PixelRenderer::getContentFontScale();
    int rowStep = (int)(14 * fs);
    int startY = 8;
    int sepGap = (int)(4 * fs);

    for (int i = 0; i < FIGHT_ITEM_COUNT; i++) {
        int y = startY + i * rowStep;
        bool available = (i != FIGHT_CUP) || isCupAvailable();

        uint16_t color;
        if (i == selected) {
            color = available ? PixelRenderer::YELLOW : PixelRenderer::rgb565(180, 180, 0);
        } else {
            color = available ? PixelRenderer::WHITE : PixelRenderer::GRAY;
        }

        if (i == selected) {
            uint16_t markColor = available ? PixelRenderer::YELLOW : PixelRenderer::GRAY;
            PixelRenderer::fillRect(4, y, 4, (int)(8 * fs), markColor);
        }

        PixelRenderer::drawPixelText(14, y, items[i], color, fs);

        if (i < FIGHT_ITEM_COUNT - 1) {
            PixelRenderer::fillRect(4, y + rowStep - sepGap, Hal::DISPLAY_W - 8, 1, PixelRenderer::GRAY);
        }
    }
}

void MenuScene::drawExploreList() {
    float fs = PixelRenderer::getContentFontScale();
    int rowStep = (int)(14 * fs);
    int startY = 10;
    int sepGap = (int)(4 * fs);
    bool exploreAvailable = GameEngine::ins().canExplore();
    bool night = GameEngine::ins().getTimeOfDay() == GameEngine::TIME_EVENING;
    LGFX_Sprite& canvas = Hal::ins().canvas();

    for (int i = 0; i < EXPLORE_ITEM_COUNT; i++) {
        int y = startY + i * rowStep;
        bool isBack = (i == LOCATION_BACK);
        bool forbidden = !isBack && night && (i == LOCATION_BACK_HILL);
        bool disabled = (!isBack && !exploreAvailable) || forbidden;
        uint16_t color = disabled ? PixelRenderer::GRAY : PixelRenderer::WHITE;
        if (i == selected) color = disabled ? PixelRenderer::rgb565(180, 180, 0) : PixelRenderer::YELLOW;

        if (i == selected) {
            PixelRenderer::fillRect(4, y, 4, (int)(8 * fs),
                                    disabled ? PixelRenderer::GRAY : PixelRenderer::YELLOW);
        }

        char label[24];
        const char* baseLabel = itemLabel(i, label, sizeof(label));
        PixelRenderer::drawPixelText(14, y, baseLabel, color, fs);

        if (!isBack) {
            PixelRenderer::fillRect(4, y + rowStep - sepGap, Hal::DISPLAY_W - 8, 1, PixelRenderer::GRAY);
        }
    }
}

bool MenuScene::onButton(const ButtonEvent& ev) {
    // Toast 激活时任意按键关闭弹窗
    if (toastMsg && Hal::ins().millis() < toastEndMs) {
        if (ev.action == BtnAction::PRESSED) {
            toastMsg = nullptr;
            return true;
        }
    }

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
            } else if (mode == Mode::FIGHT) {
                enterMode(Mode::MAIN);
            } else if (mode == Mode::EXPLORE) {
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
                    Bug& bug = GameEngine::ins().getBug();
                    if (bug.getFoodCount((FoodType)selected) > 0) {
                        GameEngine::ins().setFoodStyle((uint8_t)selected);
                        foodConfirmTime = GameEngine::ins().getGameNow() + FOOD_CONFIRM_MS;
                        Serial.printf("[Menu] Global food set to: %s\n", FoodTypeInfo::name((FoodType)selected));
                    }
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
        if (selected >= 0 && selected < BOWL_ITEM_COUNT - 1) {
            GameEngine::ins().setBowlStyle((uint8_t)selected);
            bug.setFoodTray(GameEngine::ins().getBowlStyle() + 1,
                            (FoodType)GameEngine::ins().getFoodStyle());
            saveSettingsNow();
            Serial.printf("[Menu] Bowl style: %s\n", GameEngine::ins().getBowlStyleName());
        } else {
            enterMode(Mode::BOX);
        }
        return;
    }

    if (mode == Mode::WOOD) {
        if (selected >= 0 && selected < WOOD_ITEM_COUNT - 1) {
            if (!bug.isWoodUnlocked((uint8_t)selected)) {
                showToast(UiStrings::WOOD_NEED_ROTTEN);
                return;
            }
            GameEngine::ins().setWoodStyle((uint8_t)selected);
            bug.setWood(GameEngine::ins().getWoodStyle());
            saveSettingsNow();
            // 若腐木尚未放置且已解锁，自动放置以便主界面立刻显示
            if (!bug.isWoodPlaced() && !bug.placeWood()) {
                showToast(UiStrings::WOOD_NEED_ROTTEN);
            }
            Serial.printf("[Menu] Wood style: %s placed=%d\n", GameEngine::ins().getWoodStyleName(),
                          bug.isWoodPlaced());
        } else {
            enterMode(Mode::BOX);
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

    if (mode == Mode::FIGHT) {
        if (selected == FIGHT_CUP) {
            Bug& bug = GameEngine::ins().getBug();
            if (bug.getStage() != Stage::ADULT) {
                showToast(UiStrings::CUP_NEED_ADULT);
            } else if (bug.isDead()) {
                showToast(UiStrings::CUP_BEETLE_DIED);
            } else if (bug.getHunger() < 30) {
                showToast(UiStrings::CUP_NEED_HUNGER);
            } else if (GameEngine::ins().getCupCycleState() != GameEngine::CupCycleState::REGISTER_OPEN) {
                showToast(UiStrings::CUP_NOT_OPEN);
            } else {
                nextScene = SCENE_CUP;
            }
        } else if (selected == FIGHT_CREATE) {
            GameEngine::ins().setLobbyMode(LobbyMode::LOBBY_CREATE);
            nextScene = SCENE_LOBBY;
        } else if (selected == FIGHT_SEARCH) {
            GameEngine::ins().setLobbyMode(LobbyMode::LOBBY_SEARCH);
            nextScene = SCENE_LOBBY;
        } else if (selected == FIGHT_BACK) {
            enterMode(Mode::MAIN);
        }
        return;
    }

    if (mode == Mode::EXPLORE) {
        if (selected == LOCATION_BACK) {
            enterMode(Mode::MAIN);
            return;
        }

        if (selected >= 0 && selected < EXPLORE_ITEM_COUNT - 1) {
            bool clockBlocked = !GameEngine::ins().isExploreTimeAllowed();
            if (clockBlocked) {
                showToast(UiStrings::EXPLORE_NIGHT_FORBIDDEN);
                return;
            }
            if (bug.getStage() != Stage::ADULT) {
                showToast(UiStrings::EXPLORE_NEED_ADULT);
            } else if (bug.isDead()) {
                showToast(UiStrings::EXPLORE_BEETLE_DIED);
            } else if (bug.getHunger() < 30) {
                showToast(UiStrings::EXPLORE_NEED_HUNGER);
            } else if (bug.getMot() < 50) {
                showToast(UiStrings::EXPLORE_MOT_LOW);
            } else if (!GameEngine::isExploreLimitBypassed() &&
                       GameEngine::ins().getExploreCountToday() >= GameEngine::EXPLORE_DAILY_LIMIT) {
                showToast(UiStrings::EXPLORE_DAILY_LIMIT);
            } else {
                GameEngine::ins().setExploreLocation((uint8_t)selected);
                Serial.printf("[Menu] Explore location: %s\n", GameEngine::ins().getExploreLocationName());
                nextScene = SCENE_EXPLORE;
            }
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
            enterMode(Mode::FIGHT);
            break;
        case EXPLORE: {
            enterMode(Mode::EXPLORE);
            break;
        }
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
    else if (mode == Mode::FIGHT) lastFightSelected = selected;
    else if (mode == Mode::EXPLORE) lastExploreSelected = selected;
    else lastSelected = selected;

    mode = nextMode;
    if (mode == Mode::FOOD) {
        // 默认定位到当前选中的食物（与培养缸短按 A 保持一致）
        selected = GameEngine::ins().getFoodStyle();
        if (selected >= FOOD_ITEM_COUNT - 1) selected = 0;
        foodScroll = 0.0f;       // 让列表在首帧自然滚动到选中项
        foodConfirmTime = 0;     // 清除上一次确认反馈
    }
    else if (mode == Mode::BOWL) {
        selected = GameEngine::ins().getBowlStyle();
        if (selected >= BOWL_ITEM_COUNT - 1) selected = 0;
        bowlScroll = 0.0f;
    }
    else if (mode == Mode::WOOD) {
        selected = GameEngine::ins().getWoodStyle();
        if (selected >= WOOD_ITEM_COUNT - 2) selected = 0; // 排除 Place 和 Back
        woodScroll = 0.0f;
    }
    else if (mode == Mode::BOX) selected = lastBoxSelected;
    else if (mode == Mode::FIGHT) selected = lastFightSelected;
    else if (mode == Mode::EXPLORE) {
        selected = lastExploreSelected;
        if (selected >= EXPLORE_ITEM_COUNT - 1) selected = 0;
    }
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
    if (mode == Mode::FIGHT) return FIGHT_ITEM_COUNT;
    if (mode == Mode::EXPLORE) return EXPLORE_ITEM_COUNT;
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
        if (index >= 0 && index < BOWL_ITEM_COUNT - 1) {
            return BowlAssets::NAME[index];
        }
        return UiStrings::BACK;
    }

    if (mode == Mode::WOOD) {
        if (index >= 0 && index < WOOD_ITEM_COUNT - 1) {
            return WoodAssets::NAME[index];
        }
        return UiStrings::BACK;
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

    if (mode == Mode::FIGHT) {
        switch (index) {
            case FIGHT_CUP: return UiStrings::MENU_FIGHT_CUP;
            case FIGHT_CREATE: return UiStrings::MENU_FIGHT_CREATE;
            case FIGHT_SEARCH: return UiStrings::MENU_FIGHT_SEARCH;
            case FIGHT_BACK:
            default: return UiStrings::BACK;
        }
    }

    if (mode == Mode::EXPLORE) {
        switch (index) {
            case LOCATION_PARK: return "Park";
            case LOCATION_BACK_HILL: return "Back Hill";
            case LOCATION_RIVERSIDE: return "Riverside";
            case LOCATION_OLD_WOODS: return "Old Woods";
            case LOCATION_BACK:
            default: return UiStrings::BACK;
        }
    }

    switch (index) {
        case INFO: return UiStrings::MENU_INFO;
        case FEED: return UiStrings::MENU_FEED;
        case BOX: return UiStrings::MENU_BOX;
        case FIGHT: return UiStrings::MENU_FIGHT;
        case EXPLORE: return UiStrings::MENU_EXPLORE;
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

bool MenuScene::isCupAvailable() const {
    const Bug& bug = GameEngine::ins().getBug();
    if (bug.getStage() != Stage::ADULT || bug.isDead() || bug.getHunger() < 30) return false;
    return GameEngine::ins().getCupCycleState() == GameEngine::CupCycleState::REGISTER_OPEN;
}

void MenuScene::showToast(const char* msg, uint32_t durationMs) {
    toastMsg = msg;
    toastEndMs = Hal::ins().millis() + durationMs;
}

void MenuScene::drawToast() {
    if (!toastMsg) return;
    uint64_t now = Hal::ins().millis();
    if (now >= toastEndMs) {
        toastMsg = nullptr;
        return;
    }

    float fs = PixelRenderer::getContentFontScale();
    LGFX_Sprite& canvas = Hal::ins().canvas();

    // 计算行数与最宽行
    int lineCount = 1;
    int maxLineW = 0;
    const char* p = toastMsg;
    const char* lineStart = toastMsg;
    while (true) {
        if (*p == '\n' || *p == '\0') {
            if (*p == '\n') lineCount++;
            char buf[64];
            int len = min((int)(p - lineStart), (int)sizeof(buf) - 1);
            memcpy(buf, lineStart, len);
            buf[len] = '\0';
            canvas.setTextSize(fs);
            int w = canvas.textWidth(buf);
            if (w > maxLineW) maxLineW = w;
            if (*p == '\0') break;
            lineStart = p + 1;
        }
        p++;
    }

    int padX = (int)(8 * fs);
    int padY = (int)(6 * fs);
    int lineH = (int)(12 * fs);
    int boxW = maxLineW + padX * 2;
    int boxH = lineCount * lineH + padY * 2;
    int boxX = (Hal::DISPLAY_W - boxW) / 2;
    int boxY = (Hal::DISPLAY_H - boxH) / 2;

    PixelRenderer::fillRect(boxX, boxY, boxW, boxH, PixelRenderer::rgb565(0, 0, 0));
    canvas.drawRect(boxX, boxY, boxW, boxH, PixelRenderer::WHITE);

    int y = boxY + padY;
    p = toastMsg;
    lineStart = toastMsg;
    while (true) {
        if (*p == '\n' || *p == '\0') {
            char buf[64];
            int len = min((int)(p - lineStart), (int)sizeof(buf) - 1);
            memcpy(buf, lineStart, len);
            buf[len] = '\0';
            canvas.setTextSize(fs);
            int w = canvas.textWidth(buf);
            int x = boxX + (boxW - w) / 2;
            PixelRenderer::drawPixelText(x, y, buf, PixelRenderer::WHITE, fs);
            y += lineH;
            if (*p == '\0') break;
            lineStart = p + 1;
        }
        p++;
    }
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
        bool isFood = (i < FOOD_ITEM_COUNT - 1);
        uint8_t count = isFood ? bug.getFoodCount((FoodType)i) : 1;
        bool hasStock = (count > 0);

        // 无库存的食物显示为灰色；选中时统一高亮为黄色，但无库存的黄色更灰一些
        uint16_t color;
        if (isSelected) {
            color = hasStock ? PixelRenderer::YELLOW : PixelRenderer::rgb565(180, 180, 0);
        } else {
            color = hasStock ? PixelRenderer::WHITE : PixelRenderer::GRAY;
        }

        const char* label;
        if (i == FOOD_ITEM_COUNT - 1) {
            label = UiStrings::BACK;
        } else {
            label = FoodTypeInfo::name((FoodType)i);
        }

        int textY = y + (ROW_H - (int)(8 * fs)) / 2;

        // 全局食物标记：在当前设置的食物名字前面绘制一个青色小菱形
        // 无库存的食物不能挂上菱形标记，也不能被选作全局食物
        int globalFood = GameEngine::ins().getFoodStyle();
        if (i < FOOD_ITEM_COUNT - 1 && i == globalFood && hasStock) {
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

        // 食物 icon：2x 放大，放在容器左侧
        uint16_t foodOffset = pgm_read_word(&FoodAssets::SPRITE_FRAMES[idx].offset);
        uint16_t foodLength = pgm_read_word(&FoodAssets::SPRITE_FRAMES[idx].length);
        int iconScale = 2;
        int iconW = (int)(FoodAssets::FRAME_W * iconScale);
        int iconH = (int)(FoodAssets::FRAME_H * iconScale);

        const char* name = FoodTypeInfo::name(ft);
        uint8_t count = bug.getFoodCount(ft);
        char storageBuf[16];
        snprintf(storageBuf, sizeof(storageBuf), "x %d", count);

        canvas.setTextSize(fs);
        int nameW = canvas.textWidth(name);
        int storageW = canvas.textWidth(storageBuf);
        int textW = (nameW > storageW) ? nameW : storageW;

        // icon 距离左侧分割线 5px，文字紧跟 icon 右侧
        int iconX = RIGHT_X + 5;
        int iconY = 18;
        int textX = iconX + iconW + 5;
        int nameY = iconY + 2;
        int storageY = nameY + (int)(12 * fs);

        PixelRenderer::drawRgb565RleScaled(iconX, iconY,
                                           FoodAssets::FRAME_W,
                                           FoodAssets::FRAME_H,
                                           FoodAssets::SPRITE_RLE,
                                           foodOffset, foodLength, iconScale, false);

        // 名字在右上，库存在右下
        PixelRenderer::drawPixelText(textX, nameY, name, PixelRenderer::WHITE, fs);
        PixelRenderer::drawPixelText(textX, storageY, storageBuf,
                                     count > 0 ? PixelRenderer::CREAM : PixelRenderer::GRAY, fs);

        // 描述：放在容器下方，严格限制在右侧面板内
        int descX = RIGHT_X + 2;
        int descY = iconY + iconH + 12;
        PixelRenderer::drawPixelText(descX, descY,
                                     FoodTypeInfo::descLine1(ft),
                                     PixelRenderer::CREAM, fs);
        descY += (int)(12 * fs);
        PixelRenderer::drawPixelText(descX, descY,
                                     FoodTypeInfo::descLine2(ft),
                                     PixelRenderer::CREAM, fs);
    } else if (selected == FOOD_ITEM_COUNT - 1) {
        // Back 选中时的右侧提示
        PixelRenderer::drawPixelText(RIGHT_X + 20, 80, "back to menu",
                                     PixelRenderer::GRAY, fs);
    }
}


void MenuScene::drawBowlLayout() {
    Bug& bug = GameEngine::ins().getBug();
    LGFX_Sprite& canvas = Hal::ins().canvas();
    float fs = PixelRenderer::getContentFontScale();

    static constexpr int LEFT_W = 78;
    static constexpr int RIGHT_X = LEFT_W + 8;
    static constexpr int LIST_Y_START = 10;
    static constexpr int LIST_BOTTOM = Hal::DISPLAY_H - 8;
    static constexpr int VISIBLE_H = LIST_BOTTOM - LIST_Y_START;
    static constexpr int ROW_H = 22;
    static constexpr int TEXT_X = 12;

    // ---- 左侧可滚动列表 ----
    int totalH = BOWL_ITEM_COUNT * ROW_H;
    int maxScroll = (totalH > VISIBLE_H) ? (totalH - VISIBLE_H) : 0;
    int targetScroll = selected * ROW_H - VISIBLE_H / 2 + ROW_H / 2;
    if (targetScroll < 0) targetScroll = 0;
    if (targetScroll > maxScroll) targetScroll = maxScroll;

    float diff = (float)targetScroll - bowlScroll;
    if (fabsf(diff) < 0.5f) {
        bowlScroll = (float)targetScroll;
    } else {
        bowlScroll += diff * 0.25f;
    }

    canvas.drawFastVLine(LEFT_W, 8, Hal::DISPLAY_H - 16, PixelRenderer::GRAY);

    for (int i = 0; i < BOWL_ITEM_COUNT; i++) {
        int y = LIST_Y_START + i * ROW_H - (int)bowlScroll;
        if (y + ROW_H < LIST_Y_START || y > LIST_BOTTOM) continue;

        bool isSelected = (i == selected);
        bool isStyle = (i < BOWL_ITEM_COUNT - 1);
        bool unlocked = isStyle && GameEngine::ins().isBowlStyleUnlocked((uint8_t)i);

        // 未解锁的风格显示为灰色；选中时统一高亮为黄色，未解锁的黄色更灰
        uint16_t color;
        if (isSelected) {
            color = unlocked ? PixelRenderer::YELLOW : PixelRenderer::rgb565(180, 180, 0);
        } else {
            color = unlocked ? PixelRenderer::WHITE : PixelRenderer::GRAY;
        }

        const char* label;
        if (isStyle) {
            label = BowlAssets::NAME[i];
        } else {
            label = UiStrings::BACK;
        }

        int textY = y + (ROW_H - (int)(8 * fs)) / 2;

        // 当前食物盘风格菱形标记（只显示在已解锁的当前风格上）
        int currentStyle = GameEngine::ins().getBowlStyle();
        if (isStyle && i == currentStyle && unlocked) {
            int cx = TEXT_X - 8;
            int cy = textY + (int)(4 * fs);
            canvas.fillTriangle(cx, cy - 3, cx + 3, cy, cx, cy + 3, PixelRenderer::CYAN);
            canvas.fillTriangle(cx, cy - 3, cx, cy + 3, cx - 3, cy, PixelRenderer::CYAN);
        }

        PixelRenderer::drawPixelText(TEXT_X, textY, label, color, fs);
    }

    // ---- 右侧详情面板 ----
    if (selected >= 0 && selected < BOWL_ITEM_COUNT - 1) {
        int idx = selected;
        bool unlocked = GameEngine::ins().isBowlStyleUnlocked((uint8_t)idx);

        uint16_t bowlOffset = pgm_read_word(&BowlAssets::SPRITE_FRAMES[idx].offset);
        uint16_t bowlLength = pgm_read_word(&BowlAssets::SPRITE_FRAMES[idx].length);
        int iconScale = 2;
        int iconW = (int)(BowlAssets::FRAME_W * iconScale);
        int iconH = (int)(BowlAssets::FRAME_H * iconScale);

        const char* name = BowlAssets::NAME[idx];
        char storageBuf[16];
        snprintf(storageBuf, sizeof(storageBuf), "%s", unlocked ? "Ready" : "Locked");

        canvas.setTextSize(fs);
        int nameW = canvas.textWidth(name);
        int storageW = canvas.textWidth(storageBuf);
        int textW = (nameW > storageW) ? nameW : storageW;

        // 容器整体靠左对齐：icon 贴右侧面板左边缘，名字/状态紧贴 icon
        int iconX = RIGHT_X;
        int iconY = 7;
        int textX = iconX + iconW;
        int nameY = iconY + 2;
        int storageY = nameY + (int)(12 * fs);

        PixelRenderer::drawRgb565RleScaled(iconX, iconY,
                                           BowlAssets::FRAME_W,
                                           BowlAssets::FRAME_H,
                                           BowlAssets::SPRITE_RLE,
                                           bowlOffset, bowlLength, iconScale, false);

        // 名字在右上，状态在下
        PixelRenderer::drawPixelText(textX, nameY, name, PixelRenderer::WHITE, fs);
        PixelRenderer::drawPixelText(textX, storageY, storageBuf,
                                     unlocked ? PixelRenderer::CREAM : PixelRenderer::GRAY, fs);

        // 描述：三行，底部留出第三行空间
        int descX = RIGHT_X + 2;
        int descY = iconY + iconH + 12;
        PixelRenderer::drawPixelText(descX, descY,
                                     BowlAssets::DESC_LINE1[idx],
                                     PixelRenderer::CREAM, fs);
        descY += (int)(12 * fs);
        PixelRenderer::drawPixelText(descX, descY,
                                     BowlAssets::DESC_LINE2[idx],
                                     PixelRenderer::CREAM, fs);
        descY += (int)(12 * fs);
        PixelRenderer::drawPixelText(descX, descY,
                                     BowlAssets::DESC_LINE3[idx],
                                     PixelRenderer::CREAM, fs);
    } else if (selected == BOWL_ITEM_COUNT - 1) {
        PixelRenderer::drawPixelText(RIGHT_X + 20, 62, "back to box",
                                     PixelRenderer::GRAY, fs);
    }
}

void MenuScene::drawWoodLayout() {
    Bug& bug = GameEngine::ins().getBug();
    LGFX_Sprite& canvas = Hal::ins().canvas();
    float fs = PixelRenderer::getContentFontScale();

    static constexpr int LEFT_W = 78;
    static constexpr int RIGHT_X = LEFT_W + 8;
    static constexpr int LIST_Y_START = 10;
    static constexpr int LIST_BOTTOM = Hal::DISPLAY_H - 8;
    static constexpr int VISIBLE_H = LIST_BOTTOM - LIST_Y_START;
    static constexpr int ROW_H = 22;
    static constexpr int TEXT_X = 12;

    // ---- 左侧可滚动列表 ----
    int totalH = WOOD_ITEM_COUNT * ROW_H;
    int maxScroll = (totalH > VISIBLE_H) ? (totalH - VISIBLE_H) : 0;
    int targetScroll = selected * ROW_H - VISIBLE_H / 2 + ROW_H / 2;
    if (targetScroll < 0) targetScroll = 0;
    if (targetScroll > maxScroll) targetScroll = maxScroll;

    float diff = (float)targetScroll - woodScroll;
    if (fabsf(diff) < 0.5f) {
        woodScroll = (float)targetScroll;
    } else {
        woodScroll += diff * 0.25f;
    }

    canvas.drawFastVLine(LEFT_W, 8, Hal::DISPLAY_H - 16, PixelRenderer::GRAY);

    for (int i = 0; i < WOOD_ITEM_COUNT; i++) {
        int y = LIST_Y_START + i * ROW_H - (int)woodScroll;
        if (y + ROW_H < LIST_Y_START || y > LIST_BOTTOM) continue;

        bool isSelected = (i == selected);
        bool isStyle = (i < WOOD_ITEM_COUNT - 1);
        bool unlocked = isStyle && bug.isWoodUnlocked((uint8_t)i);
        uint16_t color;
        if (isSelected) {
            color = (!isStyle || unlocked) ? PixelRenderer::YELLOW : PixelRenderer::rgb565(180, 180, 0);
        } else {
            color = (!isStyle || unlocked) ? PixelRenderer::WHITE : PixelRenderer::GRAY;
        }

        const char* label;
        if (isStyle) {
            label = WoodAssets::NAME[i];
        } else {
            label = UiStrings::BACK;
        }

        int textY = y + (ROW_H - (int)(8 * fs)) / 2;

        // 当前腐木风格菱形标记
        int currentStyle = GameEngine::ins().getWoodStyle();
        if (isStyle && i == currentStyle && unlocked) {
            int cx = TEXT_X - 8;
            int cy = textY + (int)(4 * fs);
            canvas.fillTriangle(cx, cy - 3, cx + 3, cy, cx, cy + 3, PixelRenderer::CYAN);
            canvas.fillTriangle(cx, cy - 3, cx, cy + 3, cx - 3, cy, PixelRenderer::CYAN);
        }

        PixelRenderer::drawPixelText(TEXT_X, textY, label, color, fs);
    }

    // ---- 右侧详情面板 ----
    if (selected >= 0 && selected < WOOD_ITEM_COUNT - 1) {
        int idx = selected;
        bool unlocked = bug.isWoodUnlocked((uint8_t)idx);

        uint16_t woodOffset = pgm_read_word(&WoodAssets::SPRITE_FRAMES[idx].offset);
        uint16_t woodLength = pgm_read_word(&WoodAssets::SPRITE_FRAMES[idx].length);
        float iconScale = 1.0f;
        int iconW = (int)(WoodAssets::FRAME_W * iconScale);
        int iconH = (int)(WoodAssets::FRAME_H * iconScale);

        const char* name = WoodAssets::NAME[idx];
        char storageBuf[16];
        snprintf(storageBuf, sizeof(storageBuf), "%s", bug.isWoodUnlocked((uint8_t)idx) ? "Unlocked" : "Locked");

        canvas.setTextSize(fs);
        int nameW = canvas.textWidth(name);
        int storageW = canvas.textWidth(storageBuf);
        int textW = (nameW > storageW) ? nameW : storageW;

        // 容器整体居中但保持右侧边距：icon + 间距 + 右侧文字（名字在上，库存在下）
        int rightW = Hal::DISPLAY_W - RIGHT_X;
        int contentW = iconW + 5 + textW;
        int contentX = RIGHT_X + (rightW - contentW) / 2;
        if (contentX < RIGHT_X + 2) contentX = RIGHT_X + 2;
        int iconX = contentX;
        int iconY = 3;
        int textX = iconX + iconW + 5;
        // 文字块在 icon 右侧垂直居中
        int textBlockH = (int)(8 * fs) + (int)(12 * fs);
        int nameY = iconY + (iconH - textBlockH) / 2;
        int storageY = nameY + (int)(12 * fs);

        PixelRenderer::drawRgb565RleScaled(iconX, iconY,
                                           WoodAssets::FRAME_W,
                                           WoodAssets::FRAME_H,
                                           WoodAssets::SPRITE_RLE,
                                           woodOffset, woodLength, iconScale, false);
        if (!unlocked) {
            for (int yy = 0; yy < iconH; yy++) {
                for (int xx = 0; xx < iconW; xx++) {
                    if (((xx + yy) & 1) == 0) {
                        PixelRenderer::fillRect(iconX + xx, iconY + yy, 1, 1, PixelRenderer::GRAY);
                    }
                }
            }
        }

        // 名字在右上，库存在右下
        PixelRenderer::drawPixelText(textX, nameY, name, unlocked ? PixelRenderer::WHITE : PixelRenderer::GRAY, fs);
        PixelRenderer::drawPixelText(textX, storageY, storageBuf,
                                     unlocked ? PixelRenderer::CREAM : PixelRenderer::GRAY, fs);

        // 描述：三行，底部留出第三行空间
        int descX = RIGHT_X + 2;
        int descY = iconY + iconH + 12;
        uint16_t descColor = unlocked ? PixelRenderer::CREAM : PixelRenderer::GRAY;
        PixelRenderer::drawPixelText(descX, descY,
                                     WoodAssets::DESC_LINE1[idx],
                                     descColor, fs);
        descY += (int)(12 * fs);
        PixelRenderer::drawPixelText(descX, descY,
                                     WoodAssets::DESC_LINE2[idx],
                                     descColor, fs);
        descY += (int)(12 * fs);
        PixelRenderer::drawPixelText(descX, descY,
                                     WoodAssets::DESC_LINE3[idx],
                                     descColor, fs);
    } else {
        PixelRenderer::drawPixelText(RIGHT_X + 20, 62, "back to box",
                                     PixelRenderer::GRAY, fs);
    }
}
