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
    };

    Mode mode = Mode::MAIN;
    int selected = 0;
    float animSelected = 0.0f;  // 纵向滚动动画位置
    float foodScroll = 0.0f;    // 食物列表滚动偏移
    float woodScroll = 0.0f;    // 腐木列表滚动偏移
    float bowlScroll = 0.0f;    // 食物盘列表滚动偏移
    uint64_t foodConfirmTime = 0; // A 键确认反馈结束时间（ms）
    static int lastSelected;
    static int lastBoxSelected;
    static int lastWoodSelected;
    static int lastBowlSelected;
    static int lastFoodSelected;
    static constexpr int MAIN_ITEM_COUNT = 7;
    static constexpr int BOX_ITEM_COUNT = 4;
    static constexpr int WOOD_ITEM_COUNT = 7; // 5 种风格 + Place + Back
    static constexpr int BOWL_ITEM_COUNT = 4; // 3 种风格 + Back
    static constexpr int FOOD_ITEM_COUNT = 7;
    static constexpr uint32_t FOOD_CONFIRM_MS = 250;

    void drawBattery();
    void drawList();
    void drawFoodLayout();
    void drawWoodLayout();
    void drawBowlLayout();
    void executeSelection();
    void enterMode(Mode nextMode);
    int itemCount() const;
    const char* itemLabel(int index, char* buf, size_t bufSize) const;
    void saveSettingsNow();

    enum MenuItem {
        INFO = 0,
        FEED,
        BOX,
        FIGHT,
        EXPLORE,
        SETTINGS,
        BACK,
    };

    enum BoxItem {
        BOX_WOOD = 0,
        BOX_BOWL,
        BOX_BG,
        BOX_BACK,
    };

    enum WoodItem {
        WOOD_TWIG = 0,
        WOOD_STACK,
        WOOD_MOSSY,
        WOOD_PALE,
        WOOD_HOLLOW,
        WOOD_PLACE,
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
};
