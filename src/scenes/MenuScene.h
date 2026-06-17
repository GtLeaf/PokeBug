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
    };

    Mode mode = Mode::MAIN;
    int selected = 0;
    float animSelected = 0.0f;  // 纵向滚动动画位置
    static int lastSelected;
    static int lastBoxSelected;
    static constexpr int MAIN_ITEM_COUNT = 6;
    static constexpr int BOX_ITEM_COUNT = 3;

    void drawBattery();
    void drawList();
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
        SETTINGS,
        BACK,
    };

    enum BoxItem {
        BOX_WOOD = 0,
        BOX_BG,
        BOX_BACK,
    };
};
