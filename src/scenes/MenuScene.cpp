#include "MenuScene.h"
#include "../core/GameEngine.h"
#include "../core/SaveManager.h"
#include "../core/UiStrings.h"
#include "../hardware/BattleLink.h"
#include "../hardware/Hal.h"
#include "../hardware/PixelRenderer.h"
#include "../game/FoodType.h"
#include "../game/ItemCatalog.h"
#include "../assets/FoodAssets.h"
#include "../assets/WoodAssets.h"
#include "../assets/BowlAssets.h"
#include "../assets/MenuAssets.h"
#include "../game/NpcGenerator.h"
#include <cmath>

int MenuScene::lastSelectedByMode[(int)MenuScene::Mode::COUNT] = {};

namespace {

constexpr uint32_t MENU_VISIT_STATUS_INTERVAL_MS = 3000;

bool careItemsNotNeeded(Stage stage) {
    return stage == Stage::EGG || stage == Stage::LARVA || stage == Stage::PUPA;
}

}

void MenuScene::onEnter() {
    mode = GameEngine::ins().getPrevSceneID() == SCENE_LOBBY ? Mode::SOCIAL : Mode::MAIN;
    Bug& bug = GameEngine::ins().getBug();
    if (careItemsNotNeeded(bug.getStage())) {
        bool changed = GameEngine::ins().getBowlStyle() != 0xFF ||
                       GameEngine::ins().getWoodStyle() != 0xFF ||
                       bug.isWoodPlaced();
        GameEngine::ins().setBowlStyle(0xFF);
        bug.setFoodTray(0, (FoodType)GameEngine::ins().getFoodStyle());
        GameEngine::ins().setWoodStyle(0xFF);
        bug.removeWood();
        if (changed) saveSettingsNow();
    }

    // 从培养缸进入时重置；从 Lobby 回来时回到 social；从子菜单返回时保持上次位置。
    if (GameEngine::ins().getPrevSceneID() == SCENE_TERRARIUM) {
        selected = 0;
    } else {
        selected = lastSelectedByMode[modeIndex(mode)];
    }
    animSelected = (float)selected;

    // Debug 阶段切换索引初始化为当前甲虫阶段
    debugStageIndex = (int)bug.getStage();
    if (debugStageIndex < 0) debugStageIndex = 0;
    if (debugStageIndex > 4) debugStageIndex = 4;
    debugTemperIndex = (int)bug.getTemperament();
    if (debugTemperIndex < 0) debugTemperIndex = 0;
    if (debugTemperIndex > 5) debugTemperIndex = 5;
    attrEditActive = false;
    lastVisitStatusMs = 0;
}

void MenuScene::onExit() {
    if (sleepTransitionActive) {
        Hal::ins().setBrightness(sleepTransitionBaseBrightness);
        sleepTransitionActive = false;
    }

    lastSelectedByMode[modeIndex(mode)] = selected;
}


SceneID MenuScene::update() {
    uint32_t nowMs = Hal::ins().millis();
    serviceVisitLink(nowMs);

    if (sleepTransitionActive) {
        uint32_t elapsed = nowMs - sleepTransitionStartMs;
        if (elapsed >= SLEEP_TRANSITION_MS) {
            Hal::ins().setBrightness(sleepTransitionBaseBrightness);
            sleepTransitionActive = false;
            return SCENE_TERRARIUM;
        }
        Hal::ins().setBrightness(sleepTransitionBrightness(elapsed));
    }
    return nextScene;
}

void MenuScene::serviceVisitLink(uint32_t nowMs) {
    GameEngine& engine = GameEngine::ins();
    if (!engine.hasActiveVisitSession()) {
        lastVisitStatusMs = 0;
        return;
    }
    if (!engine.isVisitHost()) return;

    BattleLink& link = BattleLink::ins();
    if (link.isBattlePeerSet()) {
        link.update();
        uint8_t remoteHunger = 0;
        uint8_t remoteMotivation = 0;
        while (link.takeReceivedVisitVitals(remoteHunger, remoteMotivation)) {
            engine.setVisitRemoteVitals(remoteHunger, remoteMotivation);
        }
        if (link.takeReceivedVisitRecall()) {
            engine.clearVisitSession();
            lastVisitStatusMs = 0;
            Serial.println("[Menu] Visit recalled by peer");
            return;
        }
    }

    if (!link.isBattlePeerSet() || link.isSending()) return;
    if (lastVisitStatusMs != 0 &&
        nowMs - lastVisitStatusMs < MENU_VISIT_STATUS_INTERVAL_MS) {
        return;
    }
    if (link.sendVisitStatus(engine.getVisitRemainingMs(),
                             engine.getVisitDurationMs(),
                             engine.getGameSpeedX10(),
                             true)) {
        lastVisitStatusMs = nowMs;
    }
}

void MenuScene::render() {
    PixelRenderer::fillRect(0, 0, 240, 135, PixelRenderer::rgb565(0, 0, 0));

    if (sleepTransitionActive) {
        drawSleepTransition();
        return;
    }

    if (mode == Mode::MAIN) {
        drawBattery();
    }
    if (sleepConfirmActive) {
        drawSleepConfirm();
        drawToast();
        return;
    }

    if (mode == Mode::FOOD) {
        drawFoodLayout();
    } else if (mode == Mode::WOOD) {
        drawWoodLayout();
    } else if (mode == Mode::BOWL) {
        drawBowlLayout();
    } else if (mode == Mode::TOY || mode == Mode::SOCIAL || mode == Mode::GIFT || mode == Mode::FIGHT ||
               mode == Mode::DEBUG_STATE || mode == Mode::DEBUG_ATTR) {
        drawSimpleList();
    } else if (mode == Mode::EXPLORE) {
        drawExploreList();
    } else if (mode == Mode::DEBUG) {
        drawDebugList();
    } else {
        drawList();
    }

    if (attrEditActive) {
        drawAttrEditDialog();
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
    PixelRenderer::applyTextStyle(fs);
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
        float relScale = isSelected ? 1.15f : 1.0f;
        uint16_t boxColor = isSelected ? PixelRenderer::YELLOW : PixelRenderer::GRAY;
        uint16_t descColor = isSelected ? PixelRenderer::WHITE : PixelRenderer::GRAY;

        int leftW = BOX_W;
        int iconIndex = i;

        if (mode == Mode::MAIN && iconIndex >= 0 && iconIndex < MenuAssets::MAIN_ICON_COUNT) {
            if (isSelected) {
                PixelRenderer::fillRect(boxX - 4, y - BOX_H / 2, 2, BOX_H, PixelRenderer::YELLOW);
            }

            uint16_t offset = pgm_read_word(&MenuAssets::MAIN_ICON_FRAMES[iconIndex].offset);
            uint16_t length = pgm_read_word(&MenuAssets::MAIN_ICON_FRAMES[iconIndex].length);
            float iconScale = relScale;
            int scaledIconW = (int)(MenuAssets::FRAME_W * iconScale);
            int scaledIconH = (int)(MenuAssets::FRAME_H * iconScale);
            int iconX = boxX + (ICON_SLOT_W - scaledIconW) / 2;
            int iconY = y - scaledIconH / 2;
            PixelRenderer::drawRgb565RleScaled(iconX, iconY, MenuAssets::FRAME_W, MenuAssets::FRAME_H,
                                               MenuAssets::MAIN_ICON_RLE, offset, length, iconScale);
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
        PixelRenderer::applyTextStyle(fs);
        int tw = canvas.textWidth(desc);
        int th = (int)(8 * fs);
        int descX = (boxX + leftW + (int)(10 * fs) + Hal::DISPLAY_W) / 2 - 10;
        int descY = y - th / 2;
        PixelRenderer::drawPixelText(descX - tw / 2, descY, desc, descColor, fs);
    }
}

void MenuScene::drawSimpleList() {
    float fs = PixelRenderer::getContentFontScale();
    int rowStep = (int)(14 * fs);
    int startY = 8;
    int sepGap = (int)(4 * fs);
    int count = itemCount();
    LGFX_Sprite& canvas = Hal::ins().canvas();

    for (int i = 0; i < count; i++) {
        int y = startY + i * rowStep;

        uint16_t color = (i == selected) ? PixelRenderer::YELLOW : PixelRenderer::WHITE;

        if (i == selected) {
            PixelRenderer::fillRect(4, y, 4, (int)(8 * fs), PixelRenderer::YELLOW);
        }

        char label[24];
        int textX = 14;
        if (mode == Mode::TOY) {
            textX = 22;
            if (i < TOY_ITEM_COUNT - 1 && i == GameEngine::ins().getToyStyle()) {
                int cx = 14;
                int cy = y + (int)(4 * fs);
                canvas.fillTriangle(cx, cy - 3, cx + 3, cy, cx, cy + 3, PixelRenderer::CYAN);
                canvas.fillTriangle(cx, cy - 3, cx, cy + 3, cx - 3, cy, PixelRenderer::CYAN);
            }
        }
        PixelRenderer::drawPixelText(textX, y, itemLabel(i, label, sizeof(label)), color, fs);

        if (i < count - 1) {
            PixelRenderer::fillRect(4, y + rowStep - sepGap, Hal::DISPLAY_W - 8, 1, PixelRenderer::GRAY);
        }
    }
}

void MenuScene::drawFightList() {
    drawSimpleList();
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
        bool isCup = (i == LOCATION_CUP);
        bool forbidden = !isBack && !isCup && night && (i == LOCATION_BACK_HILL);
        bool disabled = isCup ? !isCupAvailable() : ((!isBack && !exploreAvailable) || forbidden);
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

void MenuScene::drawDebugList() {
    float fs = PixelRenderer::getContentFontScale();
    int rowStep = (int)(14 * fs);
    int startY = 10;
    int sepGap = (int)(4 * fs);

    for (int i = 0; i < DEBUG_ITEM_COUNT; i++) {
        int y = startY + i * rowStep;
        bool isBack = (i == DEBUG_BACK);
        uint16_t color = (i == selected) ? PixelRenderer::YELLOW : PixelRenderer::WHITE;

        if (i == selected) {
            PixelRenderer::fillRect(4, y, 4, (int)(8 * fs), PixelRenderer::YELLOW);
        }

        char label[24];
        const char* baseLabel = itemLabel(i, label, sizeof(label));
        PixelRenderer::drawPixelText(14, y, baseLabel, color, fs);

        if (!isBack) {
            PixelRenderer::fillRect(4, y + rowStep - sepGap, Hal::DISPLAY_W - 8, 1, PixelRenderer::GRAY);
        }
    }
}

void MenuScene::drawAttrEditDialog() {
    LGFX_Sprite& canvas = Hal::ins().canvas();
    float fs = PixelRenderer::getContentFontScale();

    int boxW = 154;
    int boxH = 70;
    int boxX = (Hal::DISPLAY_W - boxW) / 2;
    int boxY = (Hal::DISPLAY_H - boxH) / 2;
    PixelRenderer::fillRect(boxX, boxY, boxW, boxH, PixelRenderer::rgb565(12, 12, 18));
    canvas.drawRect(boxX, boxY, boxW, boxH, PixelRenderer::WHITE);

    char title[24];
    snprintf(title, sizeof(title), UiStrings::MENU_ATTR_EDIT_TITLE_FMT,
             attrName(attrEditIndex), attrEditValue,
             GameEngine::ins().getBug().debugGetAttrCap(attrEditIndex));
    PixelRenderer::applyTextStyle(fs);
    int titleW = canvas.textWidth(title);
    PixelRenderer::drawPixelText(boxX + (boxW - titleW) / 2, boxY + 10,
                                 title, PixelRenderer::WHITE, fs);

    static constexpr int BTN_COUNT = 3;
    const char* labels[BTN_COUNT] = {UiStrings::MINUS, UiStrings::PLUS, UiStrings::YES_LOWER};
    int btnW[BTN_COUNT] = {34, 34, 46};
    int gap = 8;
    int totalW = btnW[0] + btnW[1] + btnW[2] + gap * 2;
    int x = boxX + (boxW - totalW) / 2;
    int y = boxY + 42;
    int btnH = 18;
    for (int i = 0; i < BTN_COUNT; i++) {
        bool focused = (i == attrEditButton);
        uint16_t border = focused ? PixelRenderer::YELLOW : PixelRenderer::GRAY;
        uint16_t text = focused ? PixelRenderer::YELLOW : PixelRenderer::WHITE;
        PixelRenderer::fillRect(x, y, btnW[i], btnH, PixelRenderer::rgb565(24, 24, 30));
        canvas.drawRect(x, y, btnW[i], btnH, border);
        PixelRenderer::applyTextStyle(fs);
        int tw = canvas.textWidth(labels[i]);
        PixelRenderer::drawPixelText(x + (btnW[i] - tw) / 2,
                                     y + (btnH - (int)(8 * fs)) / 2,
                                     labels[i], text, fs);
        x += btnW[i] + gap;
    }
}

bool MenuScene::onButton(const ButtonEvent& ev) {
    if (sleepTransitionActive) {
        return true;
    }

    if (attrEditActive) {
        return handleAttrEditButton(ev);
    }

    // Toast 激活时任意按键关闭弹窗
    if (toastMsg && Hal::ins().millis() < toastEndMs) {
        if (ev.action == BtnAction::PRESSED) {
            toastMsg = nullptr;
            return true;
        }
    }

    // Sleep 确认对话框
    if (sleepConfirmActive) {
        if (ev.action == BtnAction::PRESSED) {
            if (ev.btn == 0) {
                // A：确认睡觉
                executeSleep();
            } else if (ev.btn == 1) {
                // B：取消
                sleepConfirmActive = false;
            }
            return true;
        }
        return false;
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
            } else if (mode == Mode::TOY) {
                enterMode(Mode::BOX);
            } else if (mode == Mode::BOWL) {
                enterMode(Mode::BOX);
            } else if (mode == Mode::WOOD) {
                enterMode(Mode::BOX);
            } else if (mode == Mode::BOX) {
                enterMode(Mode::MAIN);
            } else if (mode == Mode::GIFT) {
                enterMode(Mode::SOCIAL);
            } else if (mode == Mode::FIGHT) {
                enterMode(Mode::SOCIAL);
            } else if (mode == Mode::VISIT) {
                enterMode(Mode::SOCIAL);
            } else if (mode == Mode::SOCIAL) {
                enterMode(Mode::MAIN);
            } else if (mode == Mode::EXPLORE) {
                enterMode(Mode::MAIN);
            } else if (mode == Mode::DEBUG) {
                enterMode(Mode::MAIN);
            } else if (mode == Mode::DEBUG_STATE || mode == Mode::DEBUG_ATTR) {
                enterMode(Mode::DEBUG);
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
                // 食物子菜单：A 将当前高亮项设为全局食物（左侧菱形标记跟随），Back 返回箱子
                if (selected < FOOD_ITEM_COUNT - 1) {
                    Bug& bug = GameEngine::ins().getBug();
                    if (bug.getFoodCount((FoodType)selected) > 0) {
                        GameEngine::ins().setFoodStyle((uint8_t)selected);
                        foodConfirmTime = GameEngine::ins().getGameNow() + FOOD_CONFIRM_MS;
                        Serial.printf("[Menu] Global food set to: %s\n", FoodTypeInfo::name((FoodType)selected));
                    }
                } else {
                    enterMode(Mode::BOX);
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
    if (mode == Mode::TOY) {
        if (selected == TOY_BACK) {
            enterMode(Mode::BOX);
            return;
        }
        if (selected == TOY_NONE || selected == TOY_BALL) {
            GameEngine::ins().setToyStyle((uint8_t)selected);
            saveSettingsNow();
            Serial.printf("[Menu] Toy style: %s\n", GameEngine::ins().getToyStyleName());
        }
        return;
    }

    if (mode == Mode::FOOD) {
        // 食物子菜单的放置/返回已在 onButton 中直接处理
        if (selected == FOOD_BACK) {
            enterMode(Mode::BOX);
        }
        return;
    }

    if (mode == Mode::BOWL) {
        if (careItemsNotNeeded(bug.getStage())) {
            GameEngine::ins().setBowlStyle(0xFF);
            bug.setFoodTray(0, (FoodType)GameEngine::ins().getFoodStyle());
            saveSettingsNow();
            showToast(UiStrings::CARE_ITEM_NOT_NEEDED);
            return;
        }
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
        if (careItemsNotNeeded(bug.getStage())) {
            bug.removeWood();
            GameEngine::ins().setWoodStyle(0xFF);
            saveSettingsNow();
            showToast(UiStrings::CARE_ITEM_NOT_NEEDED);
            return;
        }
        if (selected == WOOD_NONE) {
            // 选择空选项：移除主界面的腐木
            bug.removeWood();
            GameEngine::ins().setWoodStyle(0xFF);  // 0xFF 表示未选择任何腐木风格
            saveSettingsNow();
            Serial.printf("[Menu] Wood removed\n");
        } else if (selected >= 0 && selected < WOOD_ITEM_COUNT - 1) {
            uint8_t style = (uint8_t)(selected - 1);
            if (!bug.isWoodUnlocked(style)) {
                showToast(UiStrings::WOOD_NEED_ROTTEN);
                return;
            }
            GameEngine::ins().setWoodStyle(style);
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
            case BOX_FOOD:
                enterMode(Mode::FOOD);
                break;
            case BOX_WOOD:
                if (careItemsNotNeeded(bug.getStage())) {
                    bug.removeWood();
                    GameEngine::ins().setWoodStyle(0xFF);
                    saveSettingsNow();
                    showToast(UiStrings::CARE_ITEM_NOT_NEEDED);
                } else {
                    enterMode(Mode::WOOD);
                }
                break;
            case BOX_BOWL:
                if (careItemsNotNeeded(bug.getStage())) {
                    GameEngine::ins().setBowlStyle(0xFF);
                    bug.setFoodTray(0, (FoodType)GameEngine::ins().getFoodStyle());
                    saveSettingsNow();
                    showToast(UiStrings::CARE_ITEM_NOT_NEEDED);
                } else {
                    enterMode(Mode::BOWL);
                }
                break;
            case BOX_BG:
                GameEngine::ins().cycleMainSceneBg();
                saveSettingsNow();
                Serial.printf("[Menu] Main scene bg: %s\n", GameEngine::ins().getMainSceneBgName());
                break;
            case BOX_TOY:
                enterMode(Mode::TOY);
                break;
            case BOX_SLEEP:
                if (bug.isDead()) {
                    showToast(UiStrings::SLEEP_BEETLE_DIED);
                } else if (!GameEngine::ins().canSleep()) {
                    showToast(UiStrings::SLEEP_TOO_EARLY);
                } else {
                    sleepConfirmActive = true;
                }
                break;
            case BOX_BACK:
                enterMode(Mode::MAIN);
                break;
        }
        return;
    }

    if (mode == Mode::SOCIAL) {
        if (selected == SOCIAL_GIFT) {
            enterMode(Mode::GIFT);
        } else if (selected == SOCIAL_FIGHT) {
            enterMode(Mode::FIGHT);
        } else if (selected == SOCIAL_VISIT) {
            enterMode(Mode::VISIT);
        } else if (selected == SOCIAL_BACK) {
            enterMode(Mode::MAIN);
        }
        return;
    }

    if (mode == Mode::GIFT) {
        if (selected == GIFT_SEND_FOOD) {
            FoodType type = (FoodType)GameEngine::ins().getFoodStyle();
            ItemId itemId = ItemCatalog::food(type);
            if (bug.getItemCount(itemId) == 0) {
                showToast(UiStrings::GIFT_NO_FOOD);
                return;
            }
            GameEngine::ins().setPendingGiftItem(itemId, 1);
            GameEngine::ins().setLobbyMode(LobbyMode::LOBBY_GIFT_SEND);
            nextScene = SCENE_LOBBY;
        } else if (selected == GIFT_RECEIVE_FOOD) {
            GameEngine::ins().clearPendingGiftItem();
            GameEngine::ins().setLobbyMode(LobbyMode::LOBBY_GIFT_RECEIVE);
            nextScene = SCENE_LOBBY;
        } else if (selected == GIFT_BACK) {
            enterMode(Mode::SOCIAL);
        }
        return;
    }

    if (mode == Mode::FIGHT) {
        if (selected == FIGHT_CREATE) {
            GameEngine::ins().setLobbyMode(LobbyMode::LOBBY_CREATE);
            nextScene = SCENE_LOBBY;
        } else if (selected == FIGHT_SEARCH) {
            GameEngine::ins().setLobbyMode(LobbyMode::LOBBY_SEARCH);
            nextScene = SCENE_LOBBY;
        } else if (selected == FIGHT_BACK) {
            enterMode(Mode::SOCIAL);
        }
        return;
    }

    if (mode == Mode::VISIT) {
        if (selected == VISIT_CREATE) {
            GameEngine::ins().setLobbyMode(LobbyMode::LOBBY_VISIT_CREATE);
            nextScene = SCENE_LOBBY;
        } else if (selected == VISIT_SEARCH) {
            GameEngine::ins().setLobbyMode(LobbyMode::LOBBY_VISIT_SEARCH);
            nextScene = SCENE_LOBBY;
        } else if (selected == VISIT_BACK) {
            enterMode(Mode::SOCIAL);
        }
        return;
    }

    if (mode == Mode::EXPLORE) {
        if (selected == LOCATION_BACK) {
            enterMode(Mode::MAIN);
            return;
        }

        if (selected == LOCATION_CUP) {
            if (bug.getStage() != Stage::ADULT) {
                showToast(UiStrings::CUP_NEED_ADULT);
            } else if (bug.isDead()) {
                showToast(UiStrings::CUP_BEETLE_DIED);
            } else if (bug.getHunger() < 30) {
                showToast(UiStrings::CUP_NEED_HUNGER);
            } else if (GameEngine::ins().getCupCycleState() != GameEngine::CupCycleState::REGISTER_OPEN) {
                showToast(cupClosedMessage());
            } else {
                nextScene = SCENE_CUP;
            }
            return;
        }

        if (selected >= 0 && selected < GameEngine::EXPLORE_LOCATION_COUNT) {
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

    if (mode == Mode::DEBUG) {
        if (selected == DEBUG_BACK) {
            enterMode(Mode::MAIN);
            return;
        }

        if (selected == DEBUG_BEETLE) {
            enterMode(Mode::DEBUG_STATE);
        } else if (selected == DEBUG_ATTR) {
            enterMode(Mode::DEBUG_ATTR);
        } else if (selected == DEBUG_NPC) {
            if (bug.isDead()) {
                showToast(UiStrings::EXPLORE_BEETLE_DIED);
            } else {
                NpcCombatant npc = NpcGenerator::generateForExplore(bug);
                GameEngine::ins().setPendingNpcBattle(npc, SCENE_MENU,
                                                      npc.tier == NpcData::Tier::LEGEND,
                                                      false, false);
                nextScene = SCENE_BATTLE;
                Serial.printf("[Menu] Debug VS NPC generated, tier=%d index=%d\n",
                              (int)npc.tier, npc.index);
            }
        }
        return;
    }

    if (mode == Mode::DEBUG_STATE) {
        if (selected == DEBUG_STATE_BACK) {
            enterMode(Mode::DEBUG);
            return;
        }

        if (selected == DEBUG_STATE_STAGE) {
            debugStageIndex = (debugStageIndex + 1) % 5;
            Stage nextStage = (Stage)debugStageIndex;
            bug.debugSetStage(nextStage, GameEngine::ins().getGameNow());
            GameEngine::ins().clearTerrariumViewState();
            if (careItemsNotNeeded(nextStage)) {
                GameEngine::ins().setBowlStyle(0xFF);
                bug.setFoodTray(0, (FoodType)GameEngine::ins().getFoodStyle());
                GameEngine::ins().setWoodStyle(0xFF);
                bug.removeWood();
                saveSettingsNow();
            }
            GameEngine::ins().forceSave();

            char toast[32];
            const char* stageName = itemLabel(selected, toast, sizeof(toast));
            showToast(stageName);
            Serial.printf("[Menu] Debug stage cycled to %d\n", debugStageIndex + 1);
        } else if (selected == DEBUG_STATE_TEMPER) {
            debugTemperIndex = (debugTemperIndex + 1) % 6;
            Temperament nextTemper = (Temperament)debugTemperIndex;
            bug.debugSetTemperament(nextTemper);
            GameEngine::ins().forceSave();

            char toast[32];
            const char* temper = itemLabel(selected, toast, sizeof(toast));
            showToast(temper);
            Serial.printf("[Menu] Debug temperament cycled to %d\n", debugTemperIndex);
        }
        return;
    }

    if (mode == Mode::DEBUG_ATTR) {
        if (selected == DEBUG_ATTR_BACK) {
            enterMode(Mode::DEBUG);
        } else if (selected >= DEBUG_ATTR_SIZ && selected <= DEBUG_ATTR_SPI) {
            openAttrEdit((uint8_t)selected);
        }
        return;
    }

    switch (selected) {
        case BOX:
            enterMode(Mode::BOX);
            break;
        case SOCIAL:
            enterMode(Mode::SOCIAL);
            break;
        case EXPLORE: {
            enterMode(Mode::EXPLORE);
            break;
        }
        case SETTINGS:
            nextScene = SCENE_SETTINGS;
            break;
        case DEBUG:
            enterMode(Mode::DEBUG);
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
    Mode fromMode = mode;
    lastSelectedByMode[modeIndex(mode)] = selected;

    mode = nextMode;
    selected = lastSelectedByMode[modeIndex(mode)];
    if (mode == Mode::FOOD) {
        selected = GameEngine::ins().getFoodStyle();
        if (selected >= FOOD_ITEM_COUNT - 1) selected = 0;
        foodScroll = 0.0f;       // 让列表在首帧自然滚动到选中项
        foodConfirmTime = 0;     // 清除上一次确认反馈
    }
    else if (mode == Mode::TOY) {
        selected = GameEngine::ins().getToyStyle();
        if (selected >= TOY_ITEM_COUNT - 1) selected = TOY_NONE;
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
    else if (mode == Mode::EXPLORE) {
        if (selected >= EXPLORE_ITEM_COUNT - 1) selected = 0;
    }
    else if (mode == Mode::DEBUG) {
        if (selected >= DEBUG_ITEM_COUNT - 1) selected = 0;
    }
    else if (mode == Mode::DEBUG_STATE) {
        debugStageIndex = (int)GameEngine::ins().getBug().getStage();
        if (debugStageIndex < 0) debugStageIndex = 0;
        if (debugStageIndex > 4) debugStageIndex = 4;
        debugTemperIndex = (int)GameEngine::ins().getBug().getTemperament();
        if (debugTemperIndex < 0) debugTemperIndex = 0;
        if (debugTemperIndex > 5) debugTemperIndex = 5;
    }

    if (shouldStartSubmenuAtFirst(fromMode, nextMode)) {
        selected = 0;
    }

    int count = itemCount();
    if (selected >= count) selected = 0;
    animSelected = (float)selected;
}

bool MenuScene::shouldStartSubmenuAtFirst(Mode fromMode, Mode toMode) const {
    switch (fromMode) {
        case Mode::MAIN:
            return toMode == Mode::BOX ||
                   toMode == Mode::SOCIAL ||
                   toMode == Mode::EXPLORE ||
                   toMode == Mode::DEBUG;
        case Mode::BOX:
            return toMode == Mode::FOOD ||
                   toMode == Mode::WOOD ||
                   toMode == Mode::BOWL ||
                   toMode == Mode::TOY;
        case Mode::SOCIAL:
            return toMode == Mode::GIFT ||
                   toMode == Mode::FIGHT ||
                   toMode == Mode::VISIT;
        case Mode::DEBUG:
            return toMode == Mode::DEBUG_STATE ||
                   toMode == Mode::DEBUG_ATTR;
        default:
            return false;
    }
}

int MenuScene::itemCount() const {
    if (mode == Mode::FOOD) return FOOD_ITEM_COUNT;
    if (mode == Mode::TOY) return TOY_ITEM_COUNT;
    if (mode == Mode::BOWL) return BOWL_ITEM_COUNT;
    if (mode == Mode::WOOD) return WOOD_ITEM_COUNT;
    if (mode == Mode::BOX) return BOX_ITEM_COUNT;
    if (mode == Mode::SOCIAL) return SOCIAL_ITEM_COUNT;
    if (mode == Mode::GIFT) return GIFT_ITEM_COUNT;
    if (mode == Mode::FIGHT) return FIGHT_ITEM_COUNT;
    if (mode == Mode::VISIT) return VISIT_ITEM_COUNT;
    if (mode == Mode::EXPLORE) return EXPLORE_ITEM_COUNT;
    if (mode == Mode::DEBUG) return DEBUG_ITEM_COUNT;
    if (mode == Mode::DEBUG_STATE) return DEBUG_STATE_ITEM_COUNT;
    if (mode == Mode::DEBUG_ATTR) return DEBUG_ATTR_ITEM_COUNT;
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

    if (mode == Mode::TOY) {
        switch (index) {
            case TOY_NONE: return UiStrings::TOY_NONE;
            case TOY_BALL: return UiStrings::TOY_BALL;
            case TOY_BACK:
            default: return UiStrings::BACK;
        }
    }

    if (mode == Mode::WOOD) {
        if (index == WOOD_NONE) {
            return UiStrings::WOOD_NONE;
        }
        if (index >= 0 && index < WOOD_ITEM_COUNT - 1) {
            return WoodAssets::NAME[index - 1];
        }
        return UiStrings::BACK;
    }

    if (mode == Mode::BOX) {
        switch (index) {
            case BOX_FOOD:
                return UiStrings::MENU_FEED;
            case BOX_WOOD:
                return UiStrings::MENU_WOOD;
            case BOX_BOWL:
                return UiStrings::MENU_BOWL;
            case BOX_BG:
                snprintf(buf, bufSize, "%s:%s", UiStrings::BG,
                         GameEngine::ins().getMainSceneBgName());
                return buf;
            case BOX_TOY:
                return UiStrings::MENU_TOY;
            case BOX_SLEEP:
                return UiStrings::MENU_SLEEP;
            case BOX_BACK:
            default:
                return UiStrings::BACK;
        }
    }

    if (mode == Mode::SOCIAL) {
        switch (index) {
            case SOCIAL_GIFT: return UiStrings::MENU_SOCIAL_GIFT;
            case SOCIAL_FIGHT: return UiStrings::MENU_SOCIAL_FIGHT;
            case SOCIAL_VISIT: return UiStrings::MENU_SOCIAL_VISIT;
            case SOCIAL_BACK:
            default: return UiStrings::BACK;
        }
    }

    if (mode == Mode::GIFT) {
        switch (index) {
            case GIFT_SEND_FOOD: return UiStrings::MENU_GIFT_SEND_FOOD;
            case GIFT_RECEIVE_FOOD: return UiStrings::MENU_GIFT_RECEIVE_FOOD;
            case GIFT_BACK:
            default: return UiStrings::BACK;
        }
    }

    if (mode == Mode::FIGHT) {
        switch (index) {
            case FIGHT_CREATE: return UiStrings::MENU_FIGHT_CREATE;
            case FIGHT_SEARCH: return UiStrings::MENU_FIGHT_SEARCH;
            case FIGHT_BACK:
            default: return UiStrings::BACK;
        }
    }

    if (mode == Mode::VISIT) {
        switch (index) {
            case VISIT_CREATE: return UiStrings::MENU_FIGHT_CREATE;
            case VISIT_SEARCH: return UiStrings::MENU_FIGHT_SEARCH;
            case VISIT_BACK:
            default: return UiStrings::BACK;
        }
    }

    if (mode == Mode::EXPLORE) {
        switch (index) {
            case LOCATION_PARK: return UiStrings::LOCATION_PARK;
            case LOCATION_BACK_HILL: return UiStrings::LOCATION_BACK_HILL;
            case LOCATION_RIVERSIDE: return UiStrings::LOCATION_RIVERSIDE;
            case LOCATION_OLD_WOODS: return UiStrings::LOCATION_OLD_WOODS;
            case LOCATION_CUP: return UiStrings::MENU_FIGHT_CUP;
            case LOCATION_BACK:
            default: return UiStrings::BACK;
        }
    }

    if (mode == Mode::DEBUG) {
        switch (index) {
            case DEBUG_BEETLE: return UiStrings::MENU_DEBUG_BEETLE;
            case DEBUG_ATTR: return UiStrings::MENU_DEBUG_ATTR;
            case DEBUG_NPC: return UiStrings::MENU_DEBUG_NPC;
            case DEBUG_BACK:
            default: return UiStrings::BACK;
        }
    }

    if (mode == Mode::DEBUG_STATE) {
        switch (index) {
            case DEBUG_STATE_STAGE:
                snprintf(buf, bufSize, UiStrings::MENU_DEBUG_STATE_STAGE_FMT,
                         stageName((Stage)debugStageIndex));
                return buf;
            case DEBUG_STATE_TEMPER:
                snprintf(buf, bufSize, UiStrings::MENU_DEBUG_STATE_TEMPER_FMT,
                         temperamentName((Temperament)debugTemperIndex));
                return buf;
            case DEBUG_STATE_BACK:
            default:
                return UiStrings::BACK;
        }
    }

    if (mode == Mode::DEBUG_ATTR) {
        if (index >= DEBUG_ATTR_SIZ && index <= DEBUG_ATTR_SPI) {
            const Bug& bug = GameEngine::ins().getBug();
            snprintf(buf, bufSize, UiStrings::MENU_DEBUG_ATTR_FMT, attrName((uint8_t)index),
                     bug.debugGetAttr((uint8_t)index),
                     bug.debugGetAttrCap((uint8_t)index));
            return buf;
        }
        return UiStrings::BACK;
    }

    switch (index) {
        case INFO: return UiStrings::MENU_INFO;
        case BOX: return UiStrings::MENU_BOX;
        case SOCIAL: return UiStrings::MENU_SOCIAL;
        case EXPLORE: return UiStrings::MENU_EXPLORE;
        case SETTINGS: return UiStrings::MENU_SETTINGS;
        case DEBUG: return UiStrings::MENU_DEBUG;
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
        GameEngine::ins().getFoodStyle(),
        GameEngine::ins().getToyStyle()
    );
}

void MenuScene::executeSleep() {
    sleepConfirmActive = false;
    if (!GameEngine::ins().sleepUntilMorning()) {
        if (GameEngine::ins().getBug().isDead()) {
            showToast(UiStrings::SLEEP_BEETLE_DIED);
        } else {
            showToast(UiStrings::SLEEP_TOO_EARLY);
        }
        Serial.println("[Menu] Sleep until morning rejected");
        return;
    }
    startSleepTransition();
    Serial.println("[Menu] Sleep until morning executed");
}

bool MenuScene::isCupAvailable() const {
    const Bug& bug = GameEngine::ins().getBug();
    if (bug.getStage() != Stage::ADULT || bug.isDead() || bug.getHunger() < 30) return false;
    return GameEngine::ins().getCupCycleState() == GameEngine::CupCycleState::REGISTER_OPEN;
}

const char* MenuScene::cupClosedMessage() {
    uint64_t now = GameEngine::ins().getGameNow();
    uint64_t cycleStart = (uint64_t)GameEngine::ins().getLastCupGameTime() * 1000ULL;
    uint64_t target = cycleStart;

    GameEngine::CupCycleState state = GameEngine::ins().getCupCycleState();
    if (state == GameEngine::CupCycleState::REGISTER_EXPIRED ||
        state == GameEngine::CupCycleState::IN_PROGRESS ||
        now >= cycleStart + GameEngine::CUP_REGISTER_MS) {
        target = cycleStart + GameEngine::CUP_CYCLE_MS;
    }
    while (target <= now) {
        target += GameEngine::CUP_CYCLE_MS;
    }

    uint64_t remaining = target - now;
    if (remaining >= GameEngine::GAME_DAY_MS) {
        uint32_t days = (uint32_t)((remaining + GameEngine::GAME_DAY_MS - 1) /
                                   GameEngine::GAME_DAY_MS);
        snprintf(cupClosedToast, sizeof(cupClosedToast),
                 UiStrings::CUP_NEXT_DAY_FMT, days,
                 days == 1 ? "" : UiStrings::PLURAL_S);
        return cupClosedToast;
    }

    static constexpr uint64_t HOUR_MS = 60ULL * 60 * 1000;
    uint32_t hours = (uint32_t)((remaining + HOUR_MS - 1) / HOUR_MS);
    if (hours > 0) {
        snprintf(cupClosedToast, sizeof(cupClosedToast),
                 UiStrings::CUP_NEXT_HOUR_FMT, hours,
                 hours == 1 ? "" : UiStrings::PLURAL_S);
        return cupClosedToast;
    }

    snprintf(cupClosedToast, sizeof(cupClosedToast), "%s", UiStrings::CUP_NEXT_SOON);
    return cupClosedToast;
}

const char* MenuScene::stageName(Stage stage) const {
    switch (stage) {
        case Stage::EGG: return UiStrings::STAGE_EGG;
        case Stage::LARVA: return UiStrings::STAGE_LARVA;
        case Stage::PUPA: return UiStrings::STAGE_PUPA;
        case Stage::JUVENILE: return UiStrings::STAGE_JUVENILE;
        case Stage::ADULT: return UiStrings::STAGE_ADULT;
        default: return UiStrings::UNKNOWN_SHORT;
    }
}

const char* MenuScene::temperamentName(Temperament temperament) const {
    switch (temperament) {
        case Temperament::SWIFT: return UiStrings::TEMP_SWIFT;
        case Temperament::RESILIENT: return UiStrings::TEMP_RESILIENT;
        case Temperament::GIANT: return UiStrings::TEMP_GIANT;
        case Temperament::BRUTE: return UiStrings::TEMP_BRUTE;
        case Temperament::BALANCED: return UiStrings::TEMP_BALANCED;
        case Temperament::SPIRIT: return UiStrings::TEMP_SPIRIT;
        default: return UiStrings::UNKNOWN_SHORT;
    }
}

const char* MenuScene::attrName(uint8_t index) const {
    switch (index) {
        case 0: return UiStrings::ATTR_SIZ;
        case 1: return UiStrings::ATTR_STR;
        case 2: return UiStrings::ATTR_END;
        case 3: return UiStrings::ATTR_SPD;
        case 4: return UiStrings::ATTR_SPI;
        default: return UiStrings::UNKNOWN_SHORT;
    }
}

void MenuScene::openAttrEdit(uint8_t index) {
    attrEditIndex = index;
    attrEditValue = roundf(GameEngine::ins().getBug().debugGetAttr(index));
    clampAttrEditValue();
    attrEditButton = ATTR_EDIT_INC;
    attrEditActive = true;
}

void MenuScene::clampAttrEditValue() {
    if (attrEditValue < 1.0f) attrEditValue = 1.0f;
    uint8_t cap = GameEngine::ins().getBug().debugGetAttrCap(attrEditIndex);
    if (attrEditValue > cap) attrEditValue = (float)cap;
}

bool MenuScene::handleAttrEditButton(const ButtonEvent& ev) {
    if (ev.action == BtnAction::LONG_PRESS && ev.btn == 1) {
        attrEditActive = false;
        return true;
    }
    if (ev.action != BtnAction::PRESSED) return true;

    if (ev.btn == 1) {
        attrEditButton++;
        if (attrEditButton > ATTR_EDIT_YES) attrEditButton = ATTR_EDIT_DEC;
        return true;
    }

    if (ev.btn == 0) {
        if (attrEditButton == ATTR_EDIT_DEC) {
            attrEditValue -= 1.0f;
            clampAttrEditValue();
        } else if (attrEditButton == ATTR_EDIT_INC) {
            attrEditValue += 1.0f;
            clampAttrEditValue();
        } else if (attrEditButton == ATTR_EDIT_YES) {
            GameEngine::ins().getBug().debugSetAttr(attrEditIndex, attrEditValue);
            GameEngine::ins().forceSave();
            attrEditActive = false;
            showToast(UiStrings::SAVED);
            Serial.printf("[Menu] Debug attr %s set to %.0f\n", attrName(attrEditIndex), attrEditValue);
        }
        return true;
    }

    return true;
}

void MenuScene::showToast(const char* msg, uint32_t durationMs) {
    if (!msg || msg[0] == '\0') {
        toastText[0] = '\0';
        toastMsg = nullptr;
        toastEndMs = 0;
        return;
    }
    snprintf(toastText, sizeof(toastText), "%s", msg);
    toastMsg = toastText;
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
            PixelRenderer::applyTextStyle(fs);
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
            PixelRenderer::applyTextStyle(fs);
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

void MenuScene::drawSleepConfirm() {
    float fs = PixelRenderer::getContentFontScale();
    LGFX_Sprite& canvas = Hal::ins().canvas();

    // 背景遮罩
    PixelRenderer::fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H,
                            PixelRenderer::rgb565(0, 0, 0));

    // 提示文字
    const char* msg = UiStrings::SLEEP_CONFIRM;
    int lineCount = 1;
    int maxLineW = 0;
    const char* p = msg;
    const char* lineStart = msg;
    while (true) {
        if (*p == '\n' || *p == '\0') {
            if (*p == '\n') lineCount++;
            char buf[64];
            int len = min((int)(p - lineStart), (int)sizeof(buf) - 1);
            memcpy(buf, lineStart, len);
            buf[len] = '\0';
            PixelRenderer::applyTextStyle(fs);
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
    int boxH = lineCount * lineH + padY * 2 + (int)(14 * fs);  // 额外空间给导航提示
    int boxX = (Hal::DISPLAY_W - boxW) / 2;
    int boxY = (Hal::DISPLAY_H - boxH) / 2;

    PixelRenderer::fillRect(boxX, boxY, boxW, boxH, PixelRenderer::rgb565(20, 20, 30));
    canvas.drawRect(boxX, boxY, boxW, boxH, PixelRenderer::WHITE);

    int y = boxY + padY;
    p = msg;
    lineStart = msg;
    while (true) {
        if (*p == '\n' || *p == '\0') {
            char buf[64];
            int len = min((int)(p - lineStart), (int)sizeof(buf) - 1);
            memcpy(buf, lineStart, len);
            buf[len] = '\0';
            PixelRenderer::applyTextStyle(fs);
            int w = canvas.textWidth(buf);
            int x = boxX + (boxW - w) / 2;
            PixelRenderer::drawPixelText(x, y, buf, PixelRenderer::WHITE, fs);
            y += lineH;
            if (*p == '\0') break;
            lineStart = p + 1;
        }
        p++;
    }

    // 导航提示
    PixelRenderer::applyTextStyle(fs);
    int navW = canvas.textWidth(UiStrings::SLEEP_NAV);
    PixelRenderer::drawPixelText(boxX + (boxW - navW) / 2,
                                 y + (int)(2 * fs),
                                 UiStrings::SLEEP_NAV,
                                 PixelRenderer::GRAY, fs);
}

void MenuScene::startSleepTransition() {
    toastMsg = nullptr;
    sleepTransitionBaseBrightness = Hal::ins().getBrightness();
    sleepTransitionStartMs = Hal::ins().millis();
    sleepTransitionActive = true;
    Hal::ins().setBrightness(sleepTransitionBaseBrightness);
}

uint8_t MenuScene::sleepTransitionBrightness(uint32_t elapsedMs) const {
    if (elapsedMs < SLEEP_FADE_MS) {
        uint32_t remaining = SLEEP_FADE_MS - elapsedMs;
        return (uint8_t)((uint32_t)sleepTransitionBaseBrightness * remaining / SLEEP_FADE_MS);
    }
    if (elapsedMs < SLEEP_FADE_MS + SLEEP_HOLD_MS) {
        return 0;
    }
    uint32_t fadeInElapsed = elapsedMs - SLEEP_FADE_MS - SLEEP_HOLD_MS;
    if (fadeInElapsed > SLEEP_FADE_MS) fadeInElapsed = SLEEP_FADE_MS;
    return (uint8_t)((uint32_t)sleepTransitionBaseBrightness * fadeInElapsed / SLEEP_FADE_MS);
}

void MenuScene::drawSleepTransition() {
    PixelRenderer::fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::BLACK);

    float fs = PixelRenderer::getContentFontScale();
    if (fs < 1.6f) fs = 1.6f;
    const char* text = UiStrings::SLEEP_ZZZ;
    LGFX_Sprite& canvas = Hal::ins().canvas();
    PixelRenderer::applyTextStyle(fs);
    int w = canvas.textWidth(text);
    int x = (Hal::DISPLAY_W - w) / 2;
    int y = (Hal::DISPLAY_H - (int)(12 * fs)) / 2;
    PixelRenderer::drawPixelText(x, y, text, PixelRenderer::WHITE, fs);
}

int MenuScene::descriptionLineStep(float fs) const {
    return (fs < 1.65f) ? 16 : 22;
}

void MenuScene::updateDescriptionScroll(int scrollKey, int maxScroll) {
    if (scrollKey != descScrollKey) {
        descScrollKey = scrollKey;
        descScroll = 0.0f;
        descScrollLastMs = Hal::ins().millis();
    }

    if (maxScroll <= 0) {
        descScroll = 0.0f;
        descScrollLastMs = Hal::ins().millis();
        return;
    }

    uint32_t now = Hal::ins().millis();
    uint32_t dt = now - descScrollLastMs;
    if (dt > 120) dt = 120;
    descScrollLastMs = now;

    float ax, ay, az;
    Hal::ins().getAccel(ax, ay, az);
    float mag = Hal::ins().getAccelMagnitude();
    if (fabsf(ay) > 1.5f || mag > 2.0f) return;

    static constexpr float TILT_DEADZONE_G = 0.22f;
    if (fabsf(ax) <= TILT_DEADZONE_G) return;

    float strength = (fabsf(ax) - TILT_DEADZONE_G) / 0.55f;
    if (strength > 1.0f) strength = 1.0f;

    float dir = (ax < 0.0f) ? 1.0f : -1.0f;
    descScroll += dir * strength * 58.0f * ((float)dt / 1000.0f);
    if (descScroll < 0.0f) descScroll = 0.0f;
    if (descScroll > (float)maxScroll) descScroll = (float)maxScroll;
}

void MenuScene::drawScrollableDescription(const char* const* lines, int lineCount,
                                          int x, int y, int w, uint16_t color,
                                          float fs, int scrollKey) {
    if (lineCount <= 0 || w <= 0) return;

    LGFX_Sprite& canvas = Hal::ins().canvas();
    PixelRenderer::applyTextStyle(fs);

    int maxLineW = 0;
    for (int i = 0; i < lineCount; ++i) {
        int lineW = canvas.textWidth(lines[i]);
        if (lineW > maxLineW) maxLineW = lineW;
    }

    int maxScroll = maxLineW > w ? maxLineW - w + 4 : 0;
    updateDescriptionScroll(scrollKey, maxScroll);

    int lineStep = descriptionLineStep(fs);
    int h = lineCount * lineStep;
    canvas.setClipRect(x, y - 1, w, h + 2);
    for (int i = 0; i < lineCount; ++i) {
        PixelRenderer::drawPixelText(x - (int)descScroll, y + i * lineStep,
                                     lines[i], color, fs);
    }
    canvas.clearClipRect();
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

        // 菜单预览保持独立视觉大小，不跟随主界面食物资源放大。
        uint16_t foodOffset = pgm_read_word(&FoodAssets::SPRITE_FRAMES[idx].offset);
        uint16_t foodLength = pgm_read_word(&FoodAssets::SPRITE_FRAMES[idx].length);
        static constexpr float FOOD_MENU_ICON_SCALE = 2.0f / 1.2f;
        float iconScale = FOOD_MENU_ICON_SCALE;
        int iconW = (int)(FoodAssets::FRAME_W * iconScale);
        int iconH = (int)(FoodAssets::FRAME_H * iconScale);

        const char* name = FoodTypeInfo::name(ft);
        uint8_t count = bug.getFoodCount(ft);
        char storageBuf[16];
        snprintf(storageBuf, sizeof(storageBuf), UiStrings::COUNT_X_FMT, count);

        PixelRenderer::applyTextStyle(fs);
        int nameW = canvas.textWidth(name);
        int storageW = canvas.textWidth(storageBuf);
        int textW = (nameW > storageW) ? nameW : storageW;

        // icon 距离左侧分割线 5px，文字紧跟 icon 右侧
        int iconX = RIGHT_X + 5;
        int iconY = 18;
        int textX = iconX + iconW + 5;
        int nameY = iconY + 2;
        int storageY = nameY + (int)(12 * fs) - 2;

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
        int descW = Hal::DISPLAY_W - descX - 2;
        const char* descLines[3] = {
            FoodTypeInfo::descLine1(ft),
            FoodTypeInfo::descLine2(ft),
            FoodTypeInfo::descLine3(ft),
        };
        drawScrollableDescription(descLines, 3, descX, descY, descW,
                                  PixelRenderer::CREAM, fs, 0x100 + idx);
    } else if (selected == FOOD_ITEM_COUNT - 1) {
        // Back 选中时的右侧提示
        PixelRenderer::drawPixelText(RIGHT_X + 20, 80, UiStrings::BACK_TO_BOX,
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
        static constexpr float BOWL_MENU_ICON_SCALE = 2.0f / 1.5f;
        float iconScale = BOWL_MENU_ICON_SCALE;
        int iconW = (int)(BowlAssets::FRAME_W * iconScale);
        int iconH = (int)(BowlAssets::FRAME_H * iconScale);

        const char* name = BowlAssets::NAME[idx];
        char storageBuf[16];
        snprintf(storageBuf, sizeof(storageBuf), "%s",
                 unlocked ? UiStrings::READY : UiStrings::LOCKED);

        PixelRenderer::applyTextStyle(fs);
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
        int descW = Hal::DISPLAY_W - descX - 2;
        const char* descLines[3] = {
            BowlAssets::DESC_LINE1[idx],
            BowlAssets::DESC_LINE2[idx],
            BowlAssets::DESC_LINE3[idx],
        };
        drawScrollableDescription(descLines, 3, descX, descY, descW,
                                  PixelRenderer::CREAM, fs, 0x200 + idx);
    } else if (selected == BOWL_ITEM_COUNT - 1) {
        PixelRenderer::drawPixelText(RIGHT_X + 20, 62, UiStrings::BACK_TO_BOX,
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
        uint8_t styleIdx = (i == WOOD_NONE) ? 0xFF : (uint8_t)(i - 1);
        bool unlocked = (i == WOOD_NONE) || (isStyle && bug.isWoodUnlocked(styleIdx));
        uint16_t color;
        if (isSelected) {
            color = (!isStyle || unlocked) ? PixelRenderer::YELLOW : PixelRenderer::rgb565(180, 180, 0);
        } else {
            color = (!isStyle || unlocked) ? PixelRenderer::WHITE : PixelRenderer::GRAY;
        }

        const char* label;
        if (i == WOOD_NONE) {
            label = UiStrings::WOOD_NONE;
        } else if (isStyle) {
            label = WoodAssets::NAME[styleIdx];
        } else {
            label = UiStrings::BACK;
        }

        int textY = y + (ROW_H - (int)(8 * fs)) / 2;

        // 当前腐木风格菱形标记（None 不显示）
        int currentStyle = (int)GameEngine::ins().getWoodStyle();
        if (i != WOOD_NONE && isStyle && styleIdx == currentStyle && unlocked) {
            int cx = TEXT_X - 8;
            int cy = textY + (int)(4 * fs);
            canvas.fillTriangle(cx, cy - 3, cx + 3, cy, cx, cy + 3, PixelRenderer::CYAN);
            canvas.fillTriangle(cx, cy - 3, cx, cy + 3, cx - 3, cy, PixelRenderer::CYAN);
        }

        PixelRenderer::drawPixelText(TEXT_X, textY, label, color, fs);
    }

    // ---- 右侧详情面板 ----
    if (selected >= 0 && selected < WOOD_ITEM_COUNT - 1) {
        if (selected == WOOD_NONE) {
            // 空选项：右侧显示简短说明
            PixelRenderer::drawPixelText(RIGHT_X + 10, 40,
                                         UiStrings::WOOD_NONE_PLACED,
                                         PixelRenderer::GRAY, fs);
            return;
        }

        int idx = selected - 1;
        bool unlocked = bug.isWoodUnlocked((uint8_t)idx);

        uint16_t woodOffset = pgm_read_word(&WoodAssets::SPRITE_FRAMES[idx].offset);
        uint16_t woodLength = pgm_read_word(&WoodAssets::SPRITE_FRAMES[idx].length);
        static constexpr float WOOD_ICON_SCALE = 1.0f / 1.5f;
        float iconScale = WOOD_ICON_SCALE;
        int iconW = (int)(WoodAssets::FRAME_W * iconScale);
        int iconH = (int)(WoodAssets::FRAME_H * iconScale);

        const char* name = WoodAssets::NAME[idx];
        char storageBuf[16];
        snprintf(storageBuf, sizeof(storageBuf), "%s",
                 bug.isWoodUnlocked((uint8_t)idx) ? UiStrings::UNLOCKED : UiStrings::LOCKED);

        PixelRenderer::applyTextStyle(fs);
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

        // 名字在右上，库存在右下
        PixelRenderer::drawPixelText(textX, nameY, name, unlocked ? PixelRenderer::WHITE : PixelRenderer::GRAY, fs);
        PixelRenderer::drawPixelText(textX, storageY, storageBuf,
                                     unlocked ? PixelRenderer::CREAM : PixelRenderer::GRAY, fs);

        // 描述：三行，底部留出第三行空间
        int descX = RIGHT_X + 2;
        int descY = iconY + iconH + 12;
        uint16_t descColor = unlocked ? PixelRenderer::CREAM : PixelRenderer::GRAY;
        int descW = Hal::DISPLAY_W - descX - 2;
        const char* descLines[3] = {
            WoodAssets::DESC_LINE1[idx],
            WoodAssets::DESC_LINE2[idx],
            WoodAssets::DESC_LINE3[idx],
        };
        drawScrollableDescription(descLines, 3, descX, descY, descW,
                                  descColor, fs, 0x300 + idx);
    } else {
        PixelRenderer::drawPixelText(RIGHT_X + 20, 62, UiStrings::BACK_TO_BOX,
                                     PixelRenderer::GRAY, fs);
    }
}
