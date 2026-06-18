#pragma once
#include "../core/Scene.h"
#include "../hardware/PixelRenderer.h"
#include "../game/Bug.h"

// 成虫行为状态
enum class AdultState {
    IDLE,   // 静止/张望
    WALK,   // 行走
    EAT,    // 在食物旁进食
    TURN,   // 原地转身
    SLIDE,  // 大角度倾斜：向低处滑落
    CLIMB   // 倾斜恢复/小角度：向高处缓慢爬行
};

// 倾斜方向
enum class TiltDir {
    NONE,   // 水平
    LEFT,   // 左倾（设备左侧向下）
    RIGHT   // 右倾（设备右侧向下）
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
    bool slideAfterTurn = false;  // 转身后进入 SLIDE
    bool climbAfterTurn = false;  // 转身后进入 CLIMB
    uint8_t turnFrameIndex = 0;
    int targetX = 120;
    int slideTargetX = 0;         // SLIDE 目标位置
    int climbTargetX = 0;         // CLIMB 目标位置
    bool tiltHighSideIsRight = false;  // 高处是否在右侧
    uint32_t stateTimer = 0;      // 当前状态已持续帧数
    uint32_t stateDuration = 0;   // 当前状态目标持续帧数

    static constexpr int GROUND_Y = 125;   // 甲虫贴地时脚所在的 Y 坐标
    static constexpr int FOOD_X = 55;      // 食物盘旁站立位置（中心点）
    static constexpr int MIN_X = 30;       // 左边界
    static constexpr int MAX_X = 171;      // 右边界，避免最大成虫帧进入右侧状态栏
    static constexpr uint32_t TURN_DURATION_FRAMES = 16;  // 20fps 下约 0.8 秒

    // 倾斜交互参数
    static constexpr float TILT_SLIDE_THRESHOLD_G = 1.0f;  // 超过此角度先向低处滑落
    static constexpr int TILT_SLIDE_DISTANCE = 30;         // 滑落距离（像素），更明显
    static constexpr int TILT_SLIDE_STEP = 2;              // 滑落：每帧移动 2 像素
    static constexpr int TILT_CLIMB_DISTANCE = 30;         // 向高处爬行距离（像素）
    static constexpr uint32_t TILT_CLIMB_SPEED_INTERVAL = 3;  // 爬坡：每 3 帧 1 像素（缓慢但可见）

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
    void startClimbOrIdle();
    bool wantsToEat();

    // 倾斜交互
    void updateTilt();
    void onTilt(TiltDir lowSide, float magnitude);
    TiltDir pendingTiltDir = TiltDir::NONE;
    TiltDir activeTiltDir = TiltDir::NONE;
    uint32_t tiltStableMs = 0;
    static constexpr uint32_t TILT_DEBOUNCE_MS = 200;
    static constexpr float TILT_THRESHOLD_G = 0.5f;

    // 戳反应
    uint32_t pokeReactionStartMs = 0;
    uint32_t pokeReactionEndMs = 0;      // 手指动画/冷却提示结束时间
    uint32_t pokeThreatenEndMs = 0;      // 成虫威吓姿态结束时间
    bool pokeReactionWasPoked = false;
    static constexpr uint32_t THREATEN_PLAY_MS = 400;   // threaten 动画播放时长
    static constexpr uint32_t THREATEN_HOLD_MS = 2500;  // 最后一帧保持时长
    static constexpr uint32_t POKE_REACTION_MS = THREATEN_PLAY_MS + THREATEN_HOLD_MS;
    bool pokeFingerFromRight = false;
    uint8_t pokeFingerFrameIndex = 0;
    int8_t pokeFingerYOffset = 0;
    void drawLarvaPoked(int x, int y, uint8_t palette);
    void drawPokeCooldownHint(int x, int y);
    void drawPokeAction();
    int getPokeTargetY() const;

    static const uint16_t PALETTE[4][2];
};
