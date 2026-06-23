#pragma once
#include "../core/Scene.h"
#include "../hardware/PixelRenderer.h"
#include "../game/Bug.h"
#include "../game/BugMind.h"
#include "../assets/HerculesAdultSprites.h"

// 成虫行为状态
enum class AdultState {
    IDLE,   // 静止/张望
    WALK,   // 行走
    EAT,    // 在食物旁进食
    TURN,   // 原地转身
    SLIDE,  // 大角度倾斜：向低处滑落
    CLIMB,  // 倾斜恢复/小角度：向高处缓慢爬行
    REST    // 夜间趴在腐木上休息
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
    void persistViewState();

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
    uint8_t eatFrameInterval = 0; // EAT 动画每帧持续帧数
    uint8_t eatBitesThisSession = 0;
    uint32_t restResumeAllowedMs = 0; // 允许重新进入夜间休息的时间戳
    uint32_t foodRefillGraceUntilMs = 0; // 刚吃空但仍饿时，等待玩家补食的时间窗
    uint32_t alertUntilMs = 0;        // 被戳后的警戒结束时间
    uint32_t lastShakeNotifyMs = 0;   // 上次 shake 通知心智的时间
    BugMind mind;

    static constexpr int GROUND_Y = 125;   // 甲虫贴地时脚所在的 Y 坐标
    static constexpr int FOOD_X = 55;      // 食物盘旁站立位置（中心点）
    static constexpr int WOOD_REST_X = 154; // 腐木上的休息位置（腐木中心）
    static constexpr uint32_t REST_WAKEUP_COOLDOWN_MIN_MS = 10000; // 唤醒后清醒最短时间 10s
    static constexpr uint32_t REST_WAKEUP_COOLDOWN_MAX_MS = 30000; // 唤醒后清醒最长时间 30s
    static constexpr int MIN_X = HerculesAdultSprites::TERRARIUM_MIN_X; // 由成虫生成脚本按最大巨体尺寸导出
    static constexpr int MAX_X = HerculesAdultSprites::TERRARIUM_MAX_X; // 由成虫生成脚本按最大巨体尺寸导出
    static constexpr uint32_t TURN_DURATION_FRAMES = 16;  // 20fps 下约 0.8 秒
    static constexpr uint32_t EAT_DURATION_MIN_FRAMES = 180; // 20fps 下约 9 秒，覆盖多口连续进食
    static constexpr uint32_t EAT_DURATION_MAX_FRAMES = 260; // 20fps 下约 13 秒，足够吃完一份食物
    static constexpr uint32_t EAT_MIN_EXIT_FRAMES = 50;      // 第一口后至少咀嚼一小段，避免一帧退出
    static constexpr uint8_t EAT_FRAME_INTERVAL_MIN = 6;     // 20fps 下约 0.3 秒
    static constexpr uint8_t EAT_FRAME_INTERVAL_MAX = 10;    // 20fps 下约 0.5 秒
    static constexpr uint8_t EAT_CONTINUE_HUNGER = 80;
    static constexpr uint32_t FOOD_REFILL_GRACE_MS = 3000;
    static constexpr uint8_t REST_GETDOWN_FRAME_INTERVAL = 8; // 入睡动作每帧约 0.4 秒
    static constexpr uint8_t REST_BREATH_FRAME_INTERVAL = 18; // 睡眠呼吸慢循环
    static constexpr uint16_t IDLE_LOOK_AROUND_CHANCE_PER_1000 = 3;
    static constexpr uint32_t ALERT_MIN_MS = 8000;
    static constexpr uint32_t ALERT_MAX_MS = 16000;

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
    void resetLocalViewState();

    void drawEgg(int x, int y, uint8_t palette);
    void drawLarva(int x, int y, uint8_t palette);
    void drawPupa(int x, int y, uint8_t palette);
    void drawAdult(int x, int y, uint8_t palette);

    void updateAdultMovement();
    void startTurn(bool targetFaceRight, bool continueWalking);
    void startWalkTo(int x);
    void enterEat();
    void enterRest();
    void setIdleDuration();
    void startClimbOrIdle();
    bool wantsToEat();
    bool wantsToRestOnWood();
    bool wantsToWander();

    // 倾斜交互
    void updateTilt();
    void onTilt(TiltDir lowSide, float magnitude);
    TiltDir pendingTiltDir = TiltDir::NONE;
    TiltDir activeTiltDir = TiltDir::NONE;
    uint32_t tiltStableMs = 0;
    static constexpr uint32_t TILT_DEBOUNCE_MS = 200;
    static constexpr float TILT_THRESHOLD_G = 0.5f;

    // 戳反应
    uint32_t pokeReactionStartMs = 0;    // 成虫威吓动画起始时间
    uint32_t pokeFingerStartMs = 0;      // 手指戳动画起始时间
    uint32_t pokeReactionEndMs = 0;      // 手指动画/冷却提示结束时间
    uint32_t pokeThreatenEndMs = 0;      // 成虫威吓姿态结束时间
    bool pokeReactionWasPoked = false;
    static constexpr uint32_t THREATEN_PLAY_MS = 400;   // threaten 动画播放时长
    static constexpr uint32_t THREATEN_HOLD_MS = 2500;  // 最后一帧保持时长
    static constexpr uint32_t THREATEN_RETURN_MS = 300; // 结束前反向播放，避免直接跳回站姿
    static constexpr uint32_t POKE_REACTION_MS = THREATEN_PLAY_MS + THREATEN_HOLD_MS + THREATEN_RETURN_MS;
    bool pokeFingerFromRight = false;
    uint8_t pokeFingerFrameIndex = 0;
    int8_t pokeFingerYOffset = 0;
    void drawLarvaPoked(int x, int y, uint8_t palette);
    void drawPokeCooldownHint(int x, int y);
    void drawPokeAction();
    int getPokeTargetY() const;

    static const uint16_t PALETTE[4][2];
};
