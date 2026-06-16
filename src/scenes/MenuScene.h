#pragma once
#include "../core/Scene.h"

// 图标菜单场景（横向循环滚动轮盘）
class MenuScene : public Scene {
public:
    MenuScene() = default;

    void onEnter() override;
    void onExit() override;
    SceneID update() override;
    void render() override;
    bool onButton(const ButtonEvent& ev) override;

private:
    int selected = 0;
    static int lastSelected;
    static constexpr int ITEM_COUNT = 5;

    void drawBattery();
    void drawCarousel();
    void executeSelection();

    enum MenuItem {
        FEED = 0,
        WOOD,
        FIGHT,
        SETTINGS,
        INFO,
    };
};
