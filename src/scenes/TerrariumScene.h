#pragma once
#include "../core/Scene.h"
#include "../hardware/PixelRenderer.h"
#include "../game/Bug.h"

// 成虫行为状态
enum class AdultState {
    IDLE,   // 静止/张望
    WALK,   // 行走
    EAT,    // 在食物旁进食
    TURN    // 原地转身
};

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

    // 成虫运动状态机
    AdultState adultState = AdultState::IDLE;
    bool faceRight = true;
    bool turnTargetFaceRight = true;
    bool walkAfterTurn = false;
    uint8_t turnFrameIndex = 0;
    int targetX = 120;
    uint32_t stateTimer = 0;      // 当前状态已持续帧数
    uint32_t stateDuration = 0;   // 当前状态目标持续帧数

    static constexpr int GROUND_Y = 125;   // 甲虫贴地时脚所在的 Y 坐标
    static constexpr int FOOD_X = 55;      // 食物盘旁站立位置（中心点）
    static constexpr int MIN_X = 30;       // 左边界
    static constexpr int MAX_X = 175;      // 右边界，避免 48x36 甲虫身体进入右侧状态栏
    static constexpr uint32_t TURN_DURATION_FRAMES = 16;  // 20fps 下约 0.8 秒

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

    void updateAdultMovement();
    void startTurn(bool targetFaceRight, bool continueWalking);
    void startWalkTo(int x);
    bool wantsToEat();

    static const uint16_t PALETTE[4][2];
};
