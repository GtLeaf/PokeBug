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
    int selected = 0;
    float animSelected = 0.0f;  // 纵向滚动动画位置
    static int lastSelected;
    static constexpr int ITEM_COUNT = 6;

    void drawBattery();
    void drawList();
    void executeSelection();

    enum MenuItem {
        FEED = 0,
        WOOD,
        FIGHT,
        SETTINGS,
        INFO,
        BACK,
    };
};
