#pragma once
#include "Scene.h"
#include "SaveManager.h"
#include "../game/Bug.h"
#include "../hardware/Hal.h"
#include "../hardware/PixelRenderer.h"

// 游戏引擎 — 主循环 + 场景调度
class GameEngine {
public:
    static GameEngine& ins();

    // 初始化（在 setup() 中调用一次）
    void begin();

    // 主循环（在 loop() 中调用）
    void run();

    // 场景切换
    void switchScene(SceneID id);

    // 获取当前场景
    Scene* currentScene() { return curScene; }

    // 获取上一个场景
    SceneID getPrevSceneID() const { return prevSceneID; }

    // 按钮分派器句柄
    int engineDispatcherHandle = -1;
    int sceneDispatcherHandle = -1;

    // 帧计数器
    uint32_t frameCount() const { return frames; }

    // 当前独角仙
    Bug& getBug() { return bug; }

    // 强制保存
    void forceSave();

    // 游戏速度（0.5 / 1 / 2 / 4 / 8… 影响虚拟时间）
    float getGameSpeed() const { return gameSpeed; }
    void setGameSpeed(float speed) { gameSpeed = speed; }

    // 虚拟游戏时间（ms）
    uint64_t getGameNow() const { return gameNow; }
    void resetGameNow() { gameNow = 0; }

    // 昼夜判断：20:00 - 次日 06:00 为夜间
    bool isNight() const {
        static constexpr uint64_t HOUR_MS = 60ULL * 60 * 1000;
        static constexpr uint64_t DAY_MS = 24ULL * HOUR_MS;
        uint64_t hour = (gameNow % DAY_MS) / HOUR_MS;
        return hour >= 20 || hour < 6;
    }

    // 全局字体缩放
    float getFontScale() const { return fontScale; }
    void setFontScale(float s) { fontScale = s; PixelRenderer::setContentFontScale(s); }

    // Idle 触发时间（档位 0-4，默认 0=30s）
    uint8_t getIdleTimeoutIndex() const { return idleTimeoutIndex; }
    void setIdleTimeoutIndex(uint8_t idx);
    uint32_t getIdleTimeoutMs() const;

    // 主场景背景主题
    enum MainSceneBg : uint8_t {
        BG_MOSS = 0,
        BG_BEGINNER = 1,
        BG_CHILD_ROOM = 2,
        BG_COUNT = 3,
    };
    uint8_t getMainSceneBg() const { return mainSceneBg; }
    void setMainSceneBg(uint8_t id);
    void cycleMainSceneBg();
    const char* getMainSceneBgName() const;

    uint8_t getWoodStyle() const { return woodStyle; }
    void setWoodStyle(uint8_t id);
    void cycleWoodStyle();
    const char* getWoodStyleName() const;

    uint8_t getBowlStyle() const { return bowlStyle; }
    void setBowlStyle(uint8_t id);
    void cycleBowlStyle();
    const char* getBowlStyleName() const;

    uint8_t getFoodStyle() const { return foodStyle; }
    void setFoodStyle(uint8_t id);
    void cycleFoodStyle();
    const char* getFoodStyleName() const;

private:
    GameEngine() = default;

    Scene* curScene = nullptr;
    SceneID curSceneID = SCENE_NONE;
    SceneID prevSceneID = SCENE_NONE;

    uint32_t lastFrameTime = 0;
    uint32_t lastInputTime = 0;
    uint32_t frames = 0;
    uint32_t idleTimer = 0;        // 无操作计时（ms）
    uint8_t idleTimeoutIndex = 0;  // Idle 触发档位（默认 0=30s）
    bool idleRendered = false;     // idle 时是否已渲染过一次

    enum class SystemState {
        ACTIVE = 0,
        IDLE,
        DEEP_SLEEP
    };
    SystemState systemState = SystemState::ACTIVE;

    Bug bug;
    uint32_t lastSaveTime = 0;

    float gameSpeed = 1.0f;
    uint64_t gameNow = 0;
    uint32_t prevRealNow = 0;
    float fontScale = 1.5f;

    uint8_t brightness = 128;
    uint8_t mainSceneBg = BG_MOSS;
    uint8_t woodStyle = 0;
    uint8_t bowlStyle = 0;
    uint8_t foodStyle = 0;

    static constexpr uint32_t ACTIVE_FRAME_MS  = 50;    // ~20fps
    static constexpr uint32_t IDLE_FRAME_MS    = 100;   // 10fps
    static constexpr uint32_t INPUT_SAMPLE_MS  = 16;    // 输入采样 60Hz
    static constexpr uint32_t AUTO_SAVE_MS     = 30000; // 30s 自动保存
    static constexpr uint32_t LIGHT_SLEEP_MS   = 60000; // 1min 后进入 Deep Sleep
    static constexpr uint32_t IMU_SAMPLE_MS    = 50;    // IMU 采样 20Hz

    uint32_t lastImuTime = 0;

    void processInput();
    void processIMU();
    uint32_t targetFrameTime() const;
    void resetIdleTimer();
};
