#pragma once
#include "../core/Scene.h"
#include "../core/TerrariumViewState.h"
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
    REST,   // 夜间趴在腐木上休息
    THREATEN, // 被戳后的威吓姿态
    ATTACK_DOWN, // 玩具蓄力下压
    ATTACK_UP    // 玩具击出上挑
};

// 倾斜方向
enum class TiltDir {
    NONE,   // 水平
    LEFT,   // 左倾（设备左侧向下）
    RIGHT   // 右倾（设备右侧向下）
};

enum class LarvaState {
    IDLE,
    EAT,
    SLEEP,
};

struct AdultBeetleActor {
    int x = 120;
    int y = 80;
    AdultState state = AdultState::IDLE;
    bool faceRight = true;
    bool turnTargetFaceRight = true;
    bool walkAfterTurn = false;
    bool slideAfterTurn = false;
    bool climbAfterTurn = false;
    bool walkTargetIsRest = false;
    uint8_t turnFrameIndex = 0;
    int targetX = 120;
    int slideTargetX = 120;
    int climbTargetX = 120;
    bool tiltHighSideIsRight = true;
    uint32_t stateTimer = 0;
    uint32_t stateDuration = 0;

    bool falling = false;
    int fromY = -30;
    int targetY = 125;
    uint32_t startMs = 0;
    uint32_t untilMs = 0;
    uint32_t nextStepMs = 0;
    uint32_t nextWanderMs = 0;
    bool turning = false;
    uint32_t turnStartMs = 0;
    uint32_t threatenStartMs = 0;
    uint32_t threatenEndMs = 0;
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
    static constexpr uint16_t CACHE_TRANSPARENT = 0xF81F;
    static constexpr int HUD_CACHE_H = 48;
    static constexpr uint8_t FRAME_CACHE_SLOTS = 28;

    struct StaticLayerCache {
        LGFX_Sprite sprite;
        bool valid = false;
        uint8_t bg = 0xFF;
        bool night = false;
        Stage stage = Stage::EGG;
        uint8_t bowlStyle = 0xFF;
        uint8_t woodStyle = 0xFF;
        uint8_t foodStyle = 0xFF;
        bool woodPlaced = false;
        bool foodInTray = false;
        uint8_t foodAmount = 0;
    };

    struct HudCache {
        LGFX_Sprite sprite;
        bool valid = false;
        bool visitHost = false;
        uint8_t fontBucket = 0;
        uint8_t weatherDayMod = 0;
        uint8_t hostHunger = 0;
        uint8_t hostMot = 0;
        uint8_t guestHunger = 0;
        uint8_t guestMot = 0;
        uint16_t remainingSec = 0;
        uint16_t durationSec = 0;
        char clock[8] = {0};
    };

    struct BeetleFrameCache {
        LGFX_Sprite sprite;
        bool valid = false;
        uint32_t lastUsed = 0;
        uintptr_t dataId = 0;
        uint16_t offset = 0;
        uint16_t length = 0;
        uint8_t frameW = 0;
        uint8_t frameH = 0;
        uint16_t scaleQ = 0;
        bool mapped = false;
        uint16_t keyMain = 0;
        uint16_t targetMain = 0;
        uint16_t keyShadow = 0;
        uint16_t targetShadow = 0;
        uint16_t keyMarking = 0;
        uint16_t targetMarking = 0;
        bool flip = false;
    };

    StaticLayerCache staticLayerCache;
    HudCache hudCache;
    BeetleFrameCache frameCache[FRAME_CACHE_SLOTS];
    uint32_t frameCacheClock = 0;

    AdultBeetleActor localActor;
    int& bugX = localActor.x;
    int& bugY = localActor.y;
    uint32_t animFrame = 0;

    uint32_t resetPressStart = 0;
    bool resetting = false;

    // 成虫运动状态机
    AdultState& adultState = localActor.state;
    bool& faceRight = localActor.faceRight;
    bool& turnTargetFaceRight = localActor.turnTargetFaceRight;
    bool& walkAfterTurn = localActor.walkAfterTurn;
    bool& slideAfterTurn = localActor.slideAfterTurn;  // 转身后进入 SLIDE
    bool& climbAfterTurn = localActor.climbAfterTurn;  // 转身后进入 CLIMB
    bool& walkTargetIsRest = localActor.walkTargetIsRest; // 当前 WALK 目标是否是睡眠点
    uint8_t& turnFrameIndex = localActor.turnFrameIndex;
    int& targetX = localActor.targetX;
    int& slideTargetX = localActor.slideTargetX;         // SLIDE 目标位置
    int& climbTargetX = localActor.climbTargetX;         // CLIMB 目标位置
    bool& tiltHighSideIsRight = localActor.tiltHighSideIsRight;  // 高处是否在右侧
    uint32_t& stateTimer = localActor.stateTimer;      // 当前状态已持续帧数
    uint32_t& stateDuration = localActor.stateDuration;   // 当前状态目标持续帧数
    uint8_t eatFrameInterval = 0; // EAT 动画每帧持续帧数
    uint8_t eatBitesThisSession = 0;
    uint32_t eatLastBiteMs = 0;   // 成虫咬食使用现实时间节奏，避免被游戏速度压缩
    LarvaState larvaState = LarvaState::IDLE;
    uint32_t larvaStateStartMs = 0;
    uint32_t larvaStateDurationMs = 0;
    int larvaTargetX = 120;
    uint32_t larvaNextStepMs = 0;
    bool larvaFaceRight = true;
    uint64_t observedLarvaEatGameMs = 0;
    uint32_t restResumeAllowedMs = 0; // 允许重新进入夜间休息的时间戳
    uint32_t foodRefillGraceUntilMs = 0; // 刚吃空但仍饿时，等待玩家补食的时间窗
    uint32_t alertUntilMs = 0;        // 被戳后的警戒结束时间
    uint32_t lastShakeNotifyMs = 0;   // 上次 shake 通知心智的时间
    BugMind mind;

    enum class ToyType : uint8_t {
        SOCCER = 0,
    };

    enum class ToyButtonInteraction : uint8_t {
        POKE = 0,       // 像未选择玩具一样戳一戳
        THROW_ARC,      // 从屏幕外抛入，带近大远小和抛物线
        DROP_DOWN,      // 从上方直接落下，适合小玩具
    };

    struct ToySpec {
        ToyType type;
        uint8_t radius;
        float mass;
        float gravity;
        float wallBounce;
        float floorBounce;
        float airDrag;
        float rollFriction;
        float baseImpulse;
        float liftImpulse;
    };

    // 玩具最小原型：默认足球色块，只和缸壁/甲虫发生伪物理碰撞
    ToyType toyType = ToyType::SOCCER;
    float toyX = 172.0f;
    float toyY = 116.0f;
    float toyVx = 0.0f;
    float toyVy = 0.0f;
    float toySpin = 0.0f;
    float toyAngle = 0.0f;
    uint32_t toyLastUpdateMs = 0;
    uint32_t toyLastHitMs = 0;
    uint32_t toyAttackStartMs = 0;
    bool toyCharging = false;
    bool visitorToyCharging = false;
    uint32_t toyChargeStartMs = 0;
    uint32_t toyChargeDurationMs = 0;
    int toyChargeDir = 1;
    uint32_t visitorToyChargeStartMs = 0;
    uint32_t visitorToyChargeDurationMs = 0;
    int visitorToyChargeDir = 1;
    uint32_t visitorToyAttackStartMs = 0;
    uint32_t toyNoCatchUntilMs = 0;
    bool toyVisible = false;
    bool toyEntryActive = false;
    ToyButtonInteraction toyEntryInteraction = ToyButtonInteraction::THROW_ARC;
    uint32_t toyEntryStartMs = 0;
    int toyThrowFromX = 120;
    int toyThrowFromY = 142;
    int toyThrowTargetX = 120;
    int toyThrowTargetY = 100;
    int toyThrowArcH = 28;
    int toyThrowCurveX = 0;
    int toyEntryTargetX = 120;
    bool toyEntryTargetVisitor = false;

    struct VisitorBug {
        bool active = false;
        AdultBeetleActor actor;
        bool& falling = actor.falling;
        int& x = actor.x;
        int& y = actor.y;
        int& fromY = actor.fromY;
        int& targetY = actor.targetY;
        uint8_t siz = 8;
        uint8_t palette = 0x80;
        uint8_t str = 1;
        uint8_t strCap = 6;
        uint8_t temperament = 0;
        bool& faceRight = actor.faceRight;
        uint32_t& startMs = actor.startMs;
        uint32_t& untilMs = actor.untilMs;
        int& targetX = actor.targetX;
        uint32_t& nextStepMs = actor.nextStepMs;
        uint32_t& nextWanderMs = actor.nextWanderMs;
        bool& turning = actor.turning;
        bool& turnTargetFaceRight = actor.turnTargetFaceRight;
        uint32_t& turnStartMs = actor.turnStartMs;
    };
    VisitorBug visitor;
    bool visitRecallConfirm = false;
    bool visitPingInFlight = false;
    uint8_t visitPingFailures = 0;
    uint32_t lastVisitPingMs = 0;
    uint32_t lastVisitStatusMs = 0;
    uint32_t lastVisitStatusRxMs = 0;
    uint32_t visitorIntentUntilMs = 0;
    bool visitorEatRequested = false;
    uint32_t lastVisitEatIntentMs = 0;
    uint32_t visitEatRetryAfterMs = 0;
    uint32_t nextVisitPlayIntentMs = 0;
    bool pendingVisitEatResult = false;
    bool pendingVisitEatSuccess = false;
    uint8_t pendingVisitEatGain = 0;
    uint8_t pendingVisitEatHunger = 0;
    uint8_t pendingVisitEatFood = 0;
    AdultState visitorLastLoggedState = AdultState::IDLE;
    bool visitorStateLogInitialized = false;
    uint32_t lastVisitorStateLogMs = 0;

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
    static constexpr uint32_t EAT_BITE_INTERVAL_MS = 2000;    // 每口至少间隔 2 秒现实时间
    static constexpr uint8_t EAT_FRAME_INTERVAL_MIN = 6;     // 20fps 下约 0.3 秒
    static constexpr uint8_t EAT_FRAME_INTERVAL_MAX = 10;    // 20fps 下约 0.5 秒
    static constexpr uint32_t LARVA_EAT_FRAME_MS = 180;
    static constexpr uint32_t LARVA_IDLE_MIN_MS = 3000;
    static constexpr uint32_t LARVA_IDLE_MAX_MS = 8000;
    static constexpr uint32_t LARVA_WALK_START_MS = 900;
    static constexpr uint32_t LARVA_WALK_STEP_MS = 80;
    static constexpr int LARVA_WALK_MIN_DELTA = 18;
    static constexpr uint32_t LARVA_EAT_MIN_MS = 45000;
    static constexpr uint32_t LARVA_EAT_MAX_MS = 90000;
    static constexpr uint32_t LARVA_SLEEP_MIN_MS = 30000;
    static constexpr uint32_t LARVA_SLEEP_MAX_MS = 90000;
    static constexpr uint8_t EAT_CONTINUE_HUNGER = 80;
    static constexpr uint32_t FOOD_REFILL_GRACE_MS = 3000;
    static constexpr uint8_t REST_GETDOWN_FRAME_INTERVAL = 8; // 入睡动作每帧约 0.4 秒
    static constexpr uint8_t REST_BREATH_FRAME_INTERVAL = 18; // 睡眠呼吸慢循环
    static constexpr uint8_t AI_DECISION_INTERVAL_FRAMES = 10;
    static constexpr uint16_t IDLE_LOOK_AROUND_CHANCE_PER_DECISION_1000 = 30;
    static constexpr uint16_t REST_WAKE_CHANCE_PER_DECISION_1000 = 40;
    static constexpr uint16_t JUVENILE_LOOK_AROUND_CHANCE_PER_DECISION_1000 = 120;
    static constexpr uint32_t ALERT_MIN_MS = 8000;
    static constexpr uint32_t ALERT_MAX_MS = 16000;

    static constexpr int TOY_TANK_LEFT = 5;
    static constexpr int TOY_TANK_RIGHT = 235;
    static constexpr int TOY_TANK_TOP = 10;
    static constexpr int TOY_TANK_BOTTOM = GROUND_Y - 1;
    static constexpr uint32_t TOY_ATTACK_UP_MS = 260;
    static constexpr uint32_t TOY_HIT_COOLDOWN_MS = 360;
    static constexpr uint32_t TOY_NO_CATCH_AFTER_HIT_MS = 1200;
    static constexpr uint32_t TOY_NO_CATCH_AFTER_REBOUND_MS = 650;
    static constexpr uint32_t TOY_THROW_MS = 520;
    static constexpr uint32_t TOY_DROP_MS = 420;
    static constexpr float TOY_CATCH_MAX_SPEED = 70.0f;
    static constexpr float TOY_TILT_ACCEL = 190.0f;
    static constexpr uint32_t VISITOR_DROP_MS = 720;
    static constexpr uint32_t VISITOR_STEP_MS = 140;
    static constexpr uint32_t VISITOR_INTENT_STEP_MS = 70;
    static constexpr uint32_t VISITOR_HOST_STEP_MS = 200;
    static constexpr uint32_t VISITOR_HOST_INTENT_STEP_MS = 120;
    static constexpr uint32_t VISITOR_INTENT_MS = 1200;
    static constexpr uint32_t VISITOR_IDLE_MIN_MS = 1200;
    static constexpr uint32_t VISITOR_IDLE_MAX_MS = 3600;
    static constexpr uint32_t VISITOR_TURN_MS = 520;
    static constexpr uint32_t VISIT_PING_INTERVAL_MS = 5000;
    static constexpr uint32_t VISIT_STATUS_INTERVAL_MS = 3000;
    static constexpr uint32_t VISIT_STATUS_TIMEOUT_MS = 12000;
    static constexpr uint32_t VISIT_EAT_INTENT_INTERVAL_MS = 8000;
    static constexpr uint32_t VISIT_EAT_FAIL_RETRY_MS = 12000;
    static constexpr uint32_t VISIT_PLAY_INTENT_MIN_MS = 18000;
    static constexpr uint32_t VISIT_PLAY_INTENT_MAX_MS = 45000;
    static constexpr uint8_t VISIT_EAT_HUNGER_THRESHOLD = 80;
    static constexpr int VISITOR_EAT_DISTANCE_PX = 10;
    static constexpr uint8_t VISIT_PING_MAX_FAILURES = 3;

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
    void drawToy();
    void drawStatusBar();
    void drawVisitAwayOverlay();
    void drawVisitRecallConfirm();
    void drawDeathScreen();
    void resetLocalViewState();
    void startPendingVisitIfAny();
    void restoreVisitorFromViewState(const TerrariumViewState& saved);
    void updateVisitor(uint32_t nowMs);
    void chooseVisitorTarget();
    void scheduleVisitorWander(uint32_t nowMs);
    void guideVisitorWalkTo(int targetX, uint32_t nowMs);
    void logVisitorState(const char* reason, uint32_t nowMs);
    void beginActorTurn(AdultBeetleActor& actor, bool targetFaceRight,
                        bool continueWalking, uint32_t timer, uint32_t duration);
    void beginActorWalkTo(AdultBeetleActor& actor, int x);
    bool chooseVisitorForHostInteraction() const;
    int hostInteractionTargetX(bool targetVisitor) const;
    float hostInteractionTargetScale(bool targetVisitor) const;
    void reactVisitorToHostPoke(uint32_t nowMs);
    void applyTiltToVisitor(TiltDir dir, float magnitude);
    void updateVisitGuestLink(uint32_t nowMs);
    void updateVisitHostLink(uint32_t nowMs);
    void applyVisitIntent(uint8_t intent, uint32_t nowMs);
    void updateVisitorEating(uint32_t nowMs);
    void queueVisitEatResult(bool success, uint8_t hungerGain, uint8_t newGuestHunger, uint8_t foodType);
    bool flushVisitEatResult();
    void drawVisitor();
    void drawVisitorAdult(int x, int y);
    float visitorAdultScale() const;
    float visitorAdultDrawScale() const;
    uint16_t visitorAdultColor() const;
    void resetToy();
    void updateToyPhysics(uint32_t nowMs);
    void startToyCharge(uint32_t nowMs, int pushDir);
    void triggerToyHit(uint32_t nowMs, int pushDir, float chargeRatio);
    void reactLocalToToyImpact(uint32_t nowMs, int pushDir);
    void reactVisitorToToyImpact(uint32_t nowMs, int pushDir);
    void reboundToyFromEntryImpact(uint32_t nowMs, int pushDir, float beetleTop,
                                   const ToySpec& spec);
    void startVisitorToyCharge(uint32_t nowMs, int pushDir);
    void triggerVisitorToyHit(uint32_t nowMs, int pushDir, float chargeRatio);
    void deflectToyFromBeetle(int pushDir);
    void deflectToyFromVisitor(uint32_t nowMs, int pushDir);
    void handleToyVisitorCollision(uint32_t nowMs, const ToySpec& spec,
                                   float left, float right);
    bool startToyButtonInteraction(uint32_t nowMs, bool allowVisitorTarget = true);
    void startToyArcThrow(uint32_t nowMs, int targetX, float targetScale, bool targetVisitor);
    void startToyDrop(uint32_t nowMs, int targetX, float targetScale, bool targetVisitor);
    void finishToyEntry(uint32_t nowMs);
    void drawToyBall(int centerX, int centerY, int radius, uint8_t phase);
    void drawToyEntry();
    bool isToyEnabled() const;
    ToyType toyTypeForStyle(uint8_t style) const;
    ToyButtonInteraction toyButtonInteractionForStyle(uint8_t style) const;
    uint32_t toyEntryDurationMs() const;
    const ToySpec& currentToySpec() const;
    uint8_t toyInterestPercent(Temperament temperament) const;
    uint32_t toyChargeDurationFor(Temperament temperament) const;
    float toyStrengthPower(const Bug& bug) const;
    Temperament visitorTemperament() const;
    float visitorToyStrengthPower() const;

    void drawEgg(int x, int y, uint8_t palette);
    void drawLarva(int x, int y, uint8_t palette);
    void drawPupa(int x, int y, uint8_t palette);
    void drawAdult(int x, int y, uint8_t palette);

    void updateAdultMovement();
    void updateJuvenileMovement();
    void startTurn(bool targetFaceRight, bool continueWalking);
    void startWalkTo(int x, bool restTarget = false);
    void enterEat();
    void enterRest();
    void enterLarvaState(LarvaState nextState, uint32_t nowMs);
    void updateLarvaState(Bug& bug, uint32_t nowMs);
    void setIdleDuration();
    int pickRestTargetX();
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
    bool pokeFingerTargetVisitor = false;
    uint8_t pokeFingerFrameIndex = 0;
    int8_t pokeFingerYOffset = 0;
    void startPokeFeedback(uint32_t now, uint32_t durationMs, bool wasPoked, bool targetVisitor = false);
    void drawLarvaPoked(int x, int y, uint8_t palette);
    void drawPokeCooldownHint(int x, int y);
    void drawPokeAction();
    int getPokeTargetX() const;
    int getPokeTargetY() const;

    static const uint16_t PALETTE[4][2];

    bool ensureCacheSprite(LGFX_Sprite& sprite, int w, int h);
    void invalidateRenderCaches();
    void drawStaticLayer();
    bool staticLayerMatches() const;
    void rebuildStaticLayer();
    void drawCachedStatusBar();
    bool hudCacheMatches(const char* clockBuf) const;
    void rebuildHudCache(const char* clockBuf);
    bool drawCachedRleFrame(int drawX, int drawY,
                            uint8_t frameW, uint8_t frameH,
                            const uint16_t* data, uint16_t offset, uint16_t length,
                            float scale, bool flipSprite);
    bool drawCachedMappedRleFrame(int drawX, int drawY,
                                  uint8_t frameW, uint8_t frameH,
                                  const uint16_t* data, uint16_t offset, uint16_t length,
                                  float scale,
                                  uint16_t keyMain, uint16_t targetMain,
                                  uint16_t keyShadow, uint16_t targetShadow,
                                  uint16_t keyMarking, uint16_t targetMarking,
                                  bool flipSprite);
    bool drawCachedFrame(int drawX, int drawY,
                         uint8_t frameW, uint8_t frameH,
                         const uint16_t* data, uint16_t offset, uint16_t length,
                         float scale,
                         bool mapped,
                         uint16_t keyMain, uint16_t targetMain,
                         uint16_t keyShadow, uint16_t targetShadow,
                         uint16_t keyMarking, uint16_t targetMarking,
                         bool flipSprite);
    bool frameCacheMatches(const BeetleFrameCache& cache,
                           int cacheW, int cacheH,
                           uintptr_t dataId, uint16_t offset, uint16_t length,
                           uint8_t frameW, uint8_t frameH, uint16_t scaleQ,
                           bool mapped,
                           uint16_t keyMain, uint16_t targetMain,
                           uint16_t keyShadow, uint16_t targetShadow,
                           uint16_t keyMarking, uint16_t targetMarking,
                           bool flipSprite) const;
    BeetleFrameCache* selectFrameCacheSlot();
};
