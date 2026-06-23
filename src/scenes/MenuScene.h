#pragma once
#include "../core/Scene.h"

// 图标菜单场景（纵向列表，选中项居中）
class MenuScene : public Scene {
public:
    MenuScene() = default;

    void onEnter() override;
    void onExit() override;
    SceneID update() override;
    void render() override;
    bool onButton(const ButtonEvent& ev) override;

private:
    enum class Mode {
        MAIN,
        BOX,
        WOOD,
        BOWL,
        FOOD,
        FIGHT,
        EXPLORE,
        DEBUG,
    };

    Mode mode = Mode::MAIN;
    int selected = 0;
    float animSelected = 0.0f;  // 纵向滚动动画位置
    float foodScroll = 0.0f;    // 食物列表滚动偏移
    float woodScroll = 0.0f;    // 腐木列表滚动偏移
    float bowlScroll = 0.0f;    // 食物盘列表滚动偏移
    uint64_t foodConfirmTime = 0; // A 键确认反馈结束时间（ms）
    const char* toastMsg = nullptr;
    uint64_t toastEndMs = 0;
    static int lastSelected;
    static int lastBoxSelected;
    static int lastWoodSelected;
    static int lastBowlSelected;
    static int lastFoodSelected;
    static int lastFightSelected;
    static int lastExploreSelected;
    static int lastDebugSelected;
    static constexpr int MAIN_ITEM_COUNT = 8;
    static constexpr int BOX_ITEM_COUNT = 6;
    static constexpr int WOOD_ITEM_COUNT = 7; // None + 5 种风格 + Back
    static constexpr int BOWL_ITEM_COUNT = 4; // 3 种风格 + Back
    static constexpr int FOOD_ITEM_COUNT = 7;
    static constexpr int FIGHT_ITEM_COUNT = 4;
    static constexpr int EXPLORE_ITEM_COUNT = 5; // 4 locations + Back
    static constexpr int DEBUG_ITEM_COUNT = 6; // 5 stages + Back
    static constexpr uint32_t FOOD_CONFIRM_MS = 250;

    void drawBattery();
    void drawList();
    void drawFightList();
    void drawExploreList();
    void drawDebugList();
    void drawFoodLayout();
    void drawWoodLayout();
    void drawBowlLayout();
    void executeSelection();
    void enterMode(Mode nextMode);
    int itemCount() const;
    const char* itemLabel(int index, char* buf, size_t bufSize) const;
    void saveSettingsNow();

    bool isCupAvailable() const;
    void showToast(const char* msg, uint32_t durationMs = 2000);
    void drawToast();

    bool sleepConfirmActive = false;
    bool sleepTransitionActive = false;
    uint32_t sleepTransitionStartMs = 0;
    uint8_t sleepTransitionBaseBrightness = 128;
    static constexpr uint32_t SLEEP_FADE_MS = 500;
    static constexpr uint32_t SLEEP_HOLD_MS = 500;
    static constexpr uint32_t SLEEP_TRANSITION_MS = SLEEP_FADE_MS * 2 + SLEEP_HOLD_MS;
    void drawSleepConfirm();
    void drawSleepTransition();
    void executeSleep();
    void startSleepTransition();
    uint8_t sleepTransitionBrightness(uint32_t elapsedMs) const;

    enum MenuItem {
        INFO = 0,
        FEED,
        BOX,
        FIGHT,
        EXPLORE,
        SETTINGS,
        BACK,
        DEBUG,
    };

    enum BoxItem {
        BOX_FOOD = 0,
        BOX_WOOD,
        BOX_BOWL,
        BOX_BG,
        BOX_SLEEP,
        BOX_BACK,
    };

    enum WoodItem {
        WOOD_NONE = 0,
        WOOD_TWIG,
        WOOD_STACK,
        WOOD_MOSSY,
        WOOD_PALE,
        WOOD_HOLLOW,
        WOOD_BACK,
    };

    enum BowlItem {
        BOWL_LOW = 0,
        BOWL_BLOCK,
        BOWL_ROOT,
        BOWL_BACK,
    };

    enum FoodItem {
        FOOD_DROP = 0,
        FOOD_CUBE,
        FOOD_SLICE,
        FOOD_CITRUS,
        FOOD_JELLY,
        FOOD_BERRY,
        FOOD_BACK,
    };

    enum FightItem {
        FIGHT_CUP = 0,
        FIGHT_CREATE,
        FIGHT_SEARCH,
        FIGHT_BACK,
    };

    enum ExploreItem {
        LOCATION_PARK = 0,
        LOCATION_BACK_HILL,
        LOCATION_RIVERSIDE,
        LOCATION_OLD_WOODS,
        LOCATION_BACK,
    };

    enum DebugItem {
        DEBUG_EGG = 0,
        DEBUG_LARVA,
        DEBUG_PUPA,
        DEBUG_JUVENILE,
        DEBUG_ADULT,
        DEBUG_BACK,
    };
};
