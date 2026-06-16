#pragma once
#include "../core/Scene.h"
#include "../hardware/Hal.h"
#include "../hardware/PixelRenderer.h"

// 设置界面 — 亮度 / 字体 / 游戏速度 / idle 时间 / 重置存档 / 返回
class SettingsScene : public Scene {
public:
    void onEnter() override;
    void onExit() override;
    SceneID update() override;
    void render() override;
    bool onButton(const ButtonEvent& ev) override;

private:
    int8_t cursor = 0;
    static constexpr int8_t ITEM_COUNT = 6;
    bool confirmReset = false;
    int scrollY = 0;  // 纵向滚动偏移

    enum Item {
        ITEM_BRIGHTNESS = 0,
        ITEM_FONT_SIZE,
        ITEM_GAME_SPEED,
        ITEM_IDLE_TIME,
        ITEM_RESET,
        ITEM_BACK,
    };

    void renderMenu();
    void renderConfirmReset();
    void cycleFontSize();
    void adjustBrightness(int delta);
    void cycleGameSpeed();
    void cycleIdleTime();
    void saveSettingsNow();
};
