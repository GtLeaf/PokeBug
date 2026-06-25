#pragma once
#include "../core/Scene.h"
#include <cstddef>
#include <cstdint>

enum class Stage;
enum class Temperament : uint8_t;

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
        SOCIAL,
        GIFT,
        FIGHT,
        EXPLORE,
        DEBUG,
        DEBUG_STATE,
        DEBUG_ATTR,
    };

    Mode mode = Mode::MAIN;
    int selected = 0;
    float animSelected = 0.0f;  // 纵向滚动动画位置
    float foodScroll = 0.0f;    // 食物列表滚动偏移
    float woodScroll = 0.0f;    // 腐木列表滚动偏移
    float bowlScroll = 0.0f;    // 食物盘列表滚动偏移
    float descScroll = 0.0f;    // 右侧描述横向滚动偏移
    uint32_t descScrollLastMs = 0;
    int descScrollKey = -1;
    uint64_t foodConfirmTime = 0; // A 键确认反馈结束时间（ms）
    const char* toastMsg = nullptr;
    char toastText[64] = {0};
    uint64_t toastEndMs = 0;
    char cupClosedToast[40] = {0};
    static int lastSelected;
    static int lastBoxSelected;
    static int lastWoodSelected;
    static int lastBowlSelected;
    static int lastFoodSelected;
    static int lastSocialSelected;
    static int lastGiftSelected;
    static int lastFightSelected;
    static int lastExploreSelected;
    static int lastDebugSelected;
    static int lastDebugStateSelected;
    static int lastDebugAttrSelected;
    static constexpr int MAIN_ITEM_COUNT = 7;
    static constexpr int BOX_ITEM_COUNT = 6;
    static constexpr int WOOD_ITEM_COUNT = 7; // None + 5 种风格 + Back
    static constexpr int BOWL_ITEM_COUNT = 4; // 3 种风格 + Back
    static constexpr int FOOD_ITEM_COUNT = 7;
    static constexpr int SOCIAL_ITEM_COUNT = 3; // Gift + Fight + Back
    static constexpr int GIFT_ITEM_COUNT = 3;   // Send Food + Receive Food + Back
    static constexpr int FIGHT_ITEM_COUNT = 3;  // Create + Search + Back
    static constexpr int EXPLORE_ITEM_COUNT = 6; // 4 locations + Cup + Back
    static constexpr int DEBUG_ITEM_COUNT = 4; // Beetle + Attr + VS NPC + Back
    static constexpr int DEBUG_STATE_ITEM_COUNT = 3; // Stage + Temper + Back
    static constexpr int DEBUG_ATTR_ITEM_COUNT = 6; // 5 attrs + Back
    static constexpr uint32_t FOOD_CONFIRM_MS = 250;

    void drawBattery();
    void drawList();
    void drawSimpleList();
    void drawFightList();
    void drawExploreList();
    void drawDebugList();
    void drawFoodLayout();
    void drawWoodLayout();
    void drawBowlLayout();
    void drawScrollableDescription(const char* const* lines, int lineCount,
                                   int x, int y, int w, uint16_t color, float fs,
                                   int scrollKey);
    int descriptionLineStep(float fs) const;
    void updateDescriptionScroll(int scrollKey, int maxScroll);
    void drawAttrEditDialog();
    void executeSelection();
    void enterMode(Mode nextMode);
    int itemCount() const;
    const char* itemLabel(int index, char* buf, size_t bufSize) const;
    void saveSettingsNow();

    bool isCupAvailable() const;
    const char* cupClosedMessage();
    const char* stageName(Stage stage) const;
    const char* temperamentName(Temperament temperament) const;
    const char* attrName(uint8_t index) const;
    void openAttrEdit(uint8_t index);
    bool handleAttrEditButton(const ButtonEvent& ev);
    void clampAttrEditValue();
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
        BOX,
        SOCIAL,
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

    enum SocialItem {
        SOCIAL_GIFT = 0,
        SOCIAL_FIGHT,
        SOCIAL_BACK,
    };

    enum GiftItem {
        GIFT_SEND_FOOD = 0,
        GIFT_RECEIVE_FOOD,
        GIFT_BACK,
    };

    enum FightItem {
        FIGHT_CREATE = 0,
        FIGHT_SEARCH,
        FIGHT_BACK,
    };

    enum ExploreItem {
        LOCATION_PARK = 0,
        LOCATION_BACK_HILL,
        LOCATION_RIVERSIDE,
        LOCATION_OLD_WOODS,
        LOCATION_CUP,
        LOCATION_BACK,
    };

    enum DebugItem {
        DEBUG_BEETLE = 0,
        DEBUG_ATTR,
        DEBUG_NPC,
        DEBUG_BACK,
    };

    enum DebugStateItem {
        DEBUG_STATE_STAGE = 0,
        DEBUG_STATE_TEMPER,
        DEBUG_STATE_BACK,
    };

    enum DebugAttrItem {
        DEBUG_ATTR_SIZ = 0,
        DEBUG_ATTR_STR,
        DEBUG_ATTR_END,
        DEBUG_ATTR_SPD,
        DEBUG_ATTR_SPI,
        DEBUG_ATTR_BACK,
    };

    enum AttrEditButton {
        ATTR_EDIT_DEC = 0,
        ATTR_EDIT_INC,
        ATTR_EDIT_YES,
    };

    int debugStageIndex = 0; // 0~4 对应 Stage::EGG~ADULT
    int debugTemperIndex = 0; // 0~5 对应 Temperament
    bool attrEditActive = false;
    uint8_t attrEditIndex = 0;
    float attrEditValue = 1.0f;
    int attrEditButton = ATTR_EDIT_INC;
};
