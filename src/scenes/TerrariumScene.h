#pragma once
#include "../core/Scene.h"
#include "../hardware/PixelRenderer.h"
#include "../game/Bug.h"

// 培养缸主场景
class TerrariumScene : public Scene {
public:
    TerrariumScene() = default;

    void onEnter() override;
    void onExit() override;
    SceneID update() override;
    void render() override;
    bool onButton(const ButtonEvent& ev) override;

private:
    int bugX = 120;
    int bugY = 80;
    uint32_t animFrame = 0;

    uint32_t resetPressStart = 0;
    bool resetting = false;

    void drawBackground();
    void drawBug();
    void drawFoodTray();
    void drawWood();
    void drawStatusBar();
    void drawDeathScreen();

    void drawEgg(int x, int y, uint8_t palette);
    void drawLarva(int x, int y, uint8_t palette);
    void drawPupa(int x, int y, uint8_t palette);
    void drawAdult(int x, int y, uint8_t palette);

    static const uint16_t PALETTE[4][2];
};
