#pragma once
#include "Scene.h"
#include "SaveManager.h"
#include "TerrariumViewState.h"
#include "../game/Bug.h"
#include "../hardware/Hal.h"
#include "../hardware/PixelRenderer.h"

#include <cstddef>
#include <cstdint>
#include "../game/NpcGenerator.h"

// 大厅入口模式：由 MenuScene 写入，LobbyScene 读取
enum class LobbyMode {
    LOBBY_DEFAULT,
    LOBBY_CREATE,
    LOBBY_SEARCH,
    LOBBY_GIFT_SEND,
    LOBBY_GIFT_RECEIVE,
    LOBBY_VISIT_CREATE,
    LOBBY_VISIT_SEARCH,
};

struct VisitBugSnapshot {
    bool active = false;
    uint8_t siz = 8;
    uint8_t palette = 0x80;
    uint8_t hunger = 100;
    uint8_t motivation = 50;
    uint8_t str = 1;
    uint8_t strCap = 6;
    uint8_t temperament = 0;
};

struct VisitSession {
    bool active = false;
    bool asHost = false;
    uint32_t startMs = 0;
    uint32_t durationMs = 0;
    uint8_t speedX10 = 10;
    uint8_t localEatCount = 0;
    uint8_t localPlayCount = 0;
    uint8_t remoteEatCount = 0;
    uint8_t remotePlayCount = 0;
    VisitBugSnapshot remoteBug;
};

// 本地 NPC 对战上下文：由 MenuScene/ExploreScene/CupScene 写入，BattleScene 读取
struct PendingNpcBattle {
    bool active = false;
    SceneID returnScene = SCENE_TERRARIUM; // 对战结束后返回的场景
    uint8_t siz = 1, str = 1, end = 1, spd = 1, spi = 1;
    uint8_t mot = 50;
    uint8_t hunger = 100;
    uint8_t palette = 0;
    NpcData::Tier tier = NpcData::Tier::ROOKIE;
    bool legend = false; // 是否传说级（用于特效）
    bool resultSet = false; // 对战结果是否已写入
    bool won = false;       // 对战结果
    bool fromExplore = false;
    bool fromCup = false;
};

// 上一场本地 NPC 对战结果（供返回场景读取）
struct NpcBattleResult {
    bool valid = false;
    bool won = false;
    NpcData::Tier tier = NpcData::Tier::ROOKIE;
    bool legend = false;
    bool fromExplore = false;
    bool fromCup = false;
    bool spiBoosted = false;
};

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

    // 帧计数器
    uint32_t frameCount() const { return frames; }

    // 当前独角仙
    Bug& getBug() { return bug; }
    const Bug& getBug() const { return bug; }

    // 强制保存
    void forceSave();

    // 培养缸场景表现状态（进出菜单时恢复位置/朝向/动作）
    const TerrariumViewState& getTerrariumViewState() const { return terrariumViewState; }
    void saveTerrariumViewState(const TerrariumViewState& state) {
        terrariumViewState = state;
        terrariumViewState.valid = true;
    }
    void clearTerrariumViewState() { terrariumViewState = TerrariumViewState(); }

    // 游戏速度（1 / 2 / 4 / 8… 影响虚拟时间）
    float getGameSpeed() const { return gameSpeed; }
    void setGameSpeed(float speed) { gameSpeed = speed; }
    static float gameSpeedFromX10(uint8_t speedX10) {
        if (speedX10 == 0 || speedX10 < 5) return 1.0f;
        if (speedX10 > 80) return 8.0f;
        return speedX10 / 10.0f;
    }
    bool setGameSpeedFromX10(uint8_t speedX10) {
        float next = gameSpeedFromX10(speedX10);
        float diff = gameSpeed > next ? gameSpeed - next : next - gameSpeed;
        if (diff < 0.01f) return false;
        gameSpeed = next;
        return true;
    }
    void saveSettingsSnapshot() {
        SaveManager::ins().saveSettings(
            PixelRenderer::getContentFontScale(),
            Hal::ins().getBrightness(),
            gameSpeed,
            idleTimeoutIndex,
            mainSceneBg,
            woodStyle,
            bowlStyle,
            foodStyle,
            toyStyle
        );
    }

    // 虚拟游戏时间（ms）
    uint64_t getGameNow() const { return gameNow; }
    void resetGameNow() { gameNow = 0; }

    // 昼夜判断：21:00 - 次日 05:00 为夜间。
    bool isNight() const {
        static constexpr uint64_t HOUR_MS = 60ULL * 60 * 1000;
        static constexpr uint64_t DAY_MS = 24ULL * HOUR_MS;
        uint64_t hour = (gameNow % DAY_MS) / HOUR_MS;
        return hour >= 21 || hour < 5;
    }

    // 全局字体缩放
    float getFontScale() const { return fontScale; }
    void setFontScale(float s) {
        s = (s < 1.65f) ? 1.5f : 1.75f;
        fontScale = s;
        PixelRenderer::setContentFontScale(s);
    }

    // Idle 触发时间（档位 0-4，默认 0=30s）
    uint8_t getIdleTimeoutIndex() const { return idleTimeoutIndex; }
    void setIdleTimeoutIndex(uint8_t idx);
    uint32_t getIdleTimeoutMs() const;

    // 主场景背景主题
    enum MainSceneBg : uint8_t {
        BG_MOSS = 0,
        BG_BEGINNER = 1,
        BG_CHILD_ROOM = 2,
        BG_ENTOMOLOGIST = 3,
        BG_SCHOOL = 4,
        BG_COUNT = 5,
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
    bool isBowlStyleUnlocked(uint8_t id) const;
    const char* getBowlStyleName() const;

    uint8_t getFoodStyle() const { return foodStyle; }
    void setFoodStyle(uint8_t id);
    void cycleFoodStyle();
    const char* getFoodStyleName() const;

    enum ToyStyle : uint8_t {
        TOY_NONE = 0,
        TOY_BALL = 1,
        TOY_COUNT = 2,
    };
    uint8_t getToyStyle() const { return toyStyle; }
    void setToyStyle(uint8_t id);
    const char* getToyStyleName() const;

    enum ExploreLocation : uint8_t {
        EXPLORE_PARK = 0,
        EXPLORE_BACK_HILL = 1,
        EXPLORE_RIVERSIDE = 2,
        EXPLORE_OLD_WOODS = 3,
        EXPLORE_LOCATION_COUNT = 4,
    };
    uint8_t getExploreLocation() const { return exploreLocation; }
    void setExploreLocation(uint8_t id);
    const char* getExploreLocationName() const;

    enum TimeOfDay : uint8_t {
        TIME_MORNING = 0,
        TIME_AFTERNOON = 1,
        TIME_EVENING = 2,
    };
    uint8_t getTimeOfDay() const { return timeOfDay; }
    const char* getTimeOfDayName() const;
    const char* getTimeOfDayShortName() const;
    void getExploreClockText(char* buf, size_t bufSize) const;
    uint8_t getExploreCountToday() const { return exploreCountToday; }
    uint32_t getExploreDay() const { return exploreDay; }
    bool isExploreTimeAllowed() const;
    bool canExplore() const;
    static bool isExploreLimitBypassed();

    // 睡觉：将虚拟时间推进到下一个早晨 06:00，并更新甲虫成长与探索时钟
    bool canSleep() const;
    bool sleepUntilMorning();

    static constexpr uint8_t EXPLORE_DAILY_LIMIT = 20;
    static constexpr uint64_t EXPLORE_FINISH_ADVANCE_MS = 3ULL * 60 * 60 * 1000;
    void recordExploreFinished();
    void syncExploreClock(bool persist = false);
    uint32_t getCurrentGameDay() const;
    static uint8_t naturalExploreTimeOfDayFromMs(uint64_t gameNowMs);
    static bool isExploreTimeAllowedFromMs(uint64_t gameNowMs);

    // 对战大厅入口模式
    LobbyMode getLobbyMode() const { return lobbyMode; }
    void setLobbyMode(LobbyMode m) { lobbyMode = m; }

    // 礼物大厅暂存道具。当前菜单只写入 food，后续可扩展到 ItemCatalog 支持的其它可交换道具。
    void setPendingGiftItem(ItemId id, uint8_t amount) {
        pendingGiftItem.id = id;
        pendingGiftItem.amount = amount;
    }
    ItemStack getPendingGiftItem() const { return pendingGiftItem; }
    void clearPendingGiftItem() {
        pendingGiftItem.id = 0;
        pendingGiftItem.amount = 0;
    }

    static constexpr uint32_t VISIT_MAX_MS = 30UL * 60UL * 1000UL;
    static constexpr uint32_t VISIT_REWARD_FULL_MS = 10UL * 60UL * 1000UL;
    static constexpr float VISIT_REWARD_MAX_TOTAL = 1.20f;
    static constexpr float VISIT_REWARD_PER_EAT = 0.24f;
    static constexpr float VISIT_REWARD_PER_PLAY = 0.16f;

    void setPendingVisitBug(uint8_t siz, uint8_t palette, uint8_t hunger = 100, uint8_t motivation = 50,
                            uint8_t str = 1, uint8_t strCap = 6, uint8_t temperament = 0) {
        pendingVisitBug.active = true;
        pendingVisitBug.siz = siz;
        pendingVisitBug.palette = palette;
        pendingVisitBug.hunger = hunger;
        pendingVisitBug.motivation = motivation;
        pendingVisitBug.str = str;
        pendingVisitBug.strCap = strCap;
        pendingVisitBug.temperament = temperament;
    }
    bool hasPendingVisitBug() const { return pendingVisitBug.active; }
    VisitBugSnapshot takePendingVisitBug() {
        VisitBugSnapshot out = pendingVisitBug;
        pendingVisitBug.active = false;
        return out;
    }
    void startVisitSession(bool asHost, uint8_t siz, uint8_t palette,
                           uint8_t hunger, uint8_t motivation,
                           uint8_t str = 1,
                           uint8_t strCap = 6,
                           uint8_t temperament = 0,
                           uint8_t speedX10 = 10,
                           uint32_t durationMs = VISIT_MAX_MS) {
        visitSession.active = true;
        visitSession.asHost = asHost;
        visitSession.startMs = Hal::ins().millis();
        visitSession.durationMs = durationMs > VISIT_MAX_MS ? VISIT_MAX_MS : durationMs;
        if (speedX10 == 0) speedX10 = 10;
        visitSession.speedX10 = speedX10;
        visitSession.remoteBug.active = true;
        visitSession.remoteBug.siz = siz;
        visitSession.remoteBug.palette = palette;
        visitSession.remoteBug.hunger = hunger;
        visitSession.remoteBug.motivation = motivation;
        visitSession.remoteBug.str = str;
        visitSession.remoteBug.strCap = strCap < 1 ? 1 : strCap;
        visitSession.remoteBug.temperament = temperament;
    }
    void clearVisitSession() {
        if (!visitSession.active) {
            visitSession = VisitSession();
            return;
        }
        applyVisitRewards();
        visitSession = VisitSession();
    }
    void recordVisitLocalEat() {
        if (!visitSession.active) return;
        if (visitSession.localEatCount < 255) visitSession.localEatCount++;
    }
    void recordVisitLocalPlay() {
        if (!visitSession.active) return;
        if (visitSession.localPlayCount < 255) visitSession.localPlayCount++;
    }
    void recordVisitRemoteEat() {
        if (!visitSession.active) return;
        if (visitSession.remoteEatCount < 255) visitSession.remoteEatCount++;
    }
    void recordVisitRemotePlay() {
        if (!visitSession.active) return;
        if (visitSession.remotePlayCount < 255) visitSession.remotePlayCount++;
    }
    void syncVisitGuestRewards(uint8_t eatCount, uint8_t playCount) {
        if (!visitSession.active || visitSession.asHost) return;
        if (eatCount > visitSession.localEatCount) visitSession.localEatCount = eatCount;
        if (playCount > visitSession.localPlayCount) visitSession.localPlayCount = playCount;
    }
    uint8_t getVisitRemoteEatCount() const { return visitSession.remoteEatCount; }
    uint8_t getVisitRemotePlayCount() const { return visitSession.remotePlayCount; }
    uint8_t getVisitLocalEatCount() const { return visitSession.localEatCount; }
    uint8_t getVisitLocalPlayCount() const { return visitSession.localPlayCount; }
    void syncVisitTiming(uint32_t remainingMs, uint32_t durationMs, uint8_t speedX10) {
        if (!visitSession.active) return;
        if (durationMs == 0 || durationMs > VISIT_MAX_MS) durationMs = VISIT_MAX_MS;
        if (remainingMs > durationMs) remainingMs = durationMs;
        if (speedX10 == 0) speedX10 = 10;
        visitSession.durationMs = durationMs;
        visitSession.speedX10 = speedX10;
        uint32_t elapsedMs = durationMs - remainingMs;
        uint32_t realElapsedMs = (uint32_t)(((uint64_t)elapsedMs * 10ULL) / speedX10);
        uint32_t now = Hal::ins().millis();
        visitSession.startMs = now >= realElapsedMs ? now - realElapsedMs : 0;
    }
    bool hasActiveVisitSession() {
        if (!visitSession.active) return false;
        if (getVisitElapsedMs() >= visitSession.durationMs) {
            clearVisitSession();
            return false;
        }
        return true;
    }
    bool isVisitHost() { return hasActiveVisitSession() && visitSession.asHost; }
    bool isVisitGuest() { return hasActiveVisitSession() && !visitSession.asHost; }
    bool isGameSpeedLocked() { return hasActiveVisitSession(); }
    uint32_t getVisitRemainingMs() {
        if (!hasActiveVisitSession()) return 0;
        uint32_t elapsed = getVisitElapsedMs();
        return elapsed >= visitSession.durationMs ? 0 : visitSession.durationMs - elapsed;
    }
    uint32_t getVisitDurationMs() const { return visitSession.durationMs; }
    const VisitBugSnapshot& getVisitRemoteBug() const { return visitSession.remoteBug; }
    void setVisitRemoteHunger(uint8_t hunger) {
        if (hunger > 100) hunger = 100;
        visitSession.remoteBug.hunger = hunger;
    }
    void setVisitRemoteVitals(uint8_t hunger, uint8_t motivation) {
        if (hunger > 100) hunger = 100;
        if (motivation > 100) motivation = 100;
        visitSession.remoteBug.hunger = hunger;
        visitSession.remoteBug.motivation = motivation;
    }

    uint8_t getGameSpeedX10() const {
        float speed = gameSpeed;
        if (speed < 0.1f) speed = 1.0f;
        uint16_t encoded = (uint16_t)(speed * 10.0f + 0.5f);
        if (encoded < 1) encoded = 1;
        if (encoded > 255) encoded = 255;
        return (uint8_t)encoded;
    }
    uint32_t getVisitElapsedMs() const {
        if (!visitSession.active) return 0;
        uint32_t realElapsed = Hal::ins().millis() - visitSession.startMs;
        return (uint32_t)(((uint64_t)realElapsed * visitSession.speedX10) / 10ULL);
    }

    // 本地 NPC 对战上下文
    PendingNpcBattle& pendingNpcBattle() { return npcBattle; }
    void clearPendingNpcBattle() { npcBattle.active = false; }
    void setPendingNpcBattle(const NpcCombatant& npc, SceneID returnScene, bool legend = false, bool fromExplore = false, bool fromCup = false);

    // 上一场本地 NPC 对战结果
    NpcBattleResult& lastNpcBattleResult() { return npcResult; }
    void clearLastNpcBattleResult() { npcResult.valid = false; npcResult.spiBoosted = false; }

    // 杯赛全局数据与周期状态
    enum class CupCycleState {
        IDLE,             // 非报名期
        REGISTER_OPEN,    // 报名中，可进入杯赛
        REGISTER_EXPIRED, // 报名结束，玩家未参与
        IN_PROGRESS,      // 玩家已参赛，正在进行
    };

    uint16_t getCupSeason() const { return cupSeason; }
    void setCupSeason(uint16_t s) { cupSeason = s; }
    uint32_t getLastCupGameTime() const { return lastCupGameTime; }
    void setLastCupGameTime(uint32_t t) { lastCupGameTime = t; }
    CupCycleState getCupCycleState() const { return cupCycleState; }
    void setCupCycleState(CupCycleState s) { cupCycleState = s; }

    // 杯赛周期：7 游戏天一届，前 3 游戏天报名
    static constexpr uint64_t GAME_DAY_MS = 24ULL * 60 * 60 * 1000;
    static constexpr uint64_t CUP_CYCLE_MS = 7 * GAME_DAY_MS;
    static constexpr uint64_t CUP_REGISTER_MS = 3 * GAME_DAY_MS;

    // 当前是否在探索/对战/杯赛等不可进入 Deep Sleep 的场景
    bool isBlockDeepSleepScene() const;
    bool isGameTimePausedScene() const;

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
    TerrariumViewState terrariumViewState;
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
    uint8_t toyStyle = TOY_NONE;
    uint8_t exploreLocation = EXPLORE_PARK;
    uint8_t timeOfDay = TIME_MORNING;
    uint8_t exploreCountToday = 0;
    uint32_t exploreDay = 0;

    LobbyMode lobbyMode = LobbyMode::LOBBY_DEFAULT;
    ItemStack pendingGiftItem;
    VisitBugSnapshot pendingVisitBug;
    VisitSession visitSession;

    void applyVisitRewards() {
        uint8_t eatCount = visitSession.localEatCount;
        uint8_t playCount = visitSession.localPlayCount;
        if (eatCount == 0 && playCount == 0) return;

        uint32_t elapsedMs = getVisitElapsedMs();
        float durationRatio = (float)elapsedMs / (float)VISIT_REWARD_FULL_MS;
        if (durationRatio > 1.0f) durationRatio = 1.0f;
        if (durationRatio < 0.0f) durationRatio = 0.0f;

        float maxTotal = VISIT_REWARD_MAX_TOTAL * durationRatio;
        float interactionTotal = (float)eatCount * VISIT_REWARD_PER_EAT +
                                 (float)playCount * VISIT_REWARD_PER_PLAY;
        float rewardTotal = interactionTotal < maxTotal ? interactionTotal : maxTotal;
        if (rewardTotal <= 0.0f) return;

        uint8_t slots = 2;
        if (rewardTotal > 1.00f) slots = 5;
        else if (rewardTotal > 0.70f) slots = 4;
        else if (rewardTotal > 0.35f) slots = 3;
        bool includeMot = rewardTotal > 1.00f && random(100) < 60;
        uint8_t attrSlots = includeMot ? 5 : slots;
        if (attrSlots > 5) attrSlots = 5;

        uint16_t weights[5] = {10, 10, 10, 10, 10}; // SIZ, STR, END, SPD, SPI
        weights[0] += (uint16_t)eatCount * 10;
        weights[2] += (uint16_t)eatCount * 20;
        weights[1] += (uint16_t)playCount * 10;
        weights[3] += (uint16_t)playCount * 10;
        weights[4] += (uint16_t)playCount * 20;

        bool picked[5] = {false, false, false, false, false};
        uint8_t pickedAttrs[5] = {0, 0, 0, 0, 0};
        for (uint8_t slot = 0; slot < attrSlots; ++slot) {
            uint16_t totalWeight = 0;
            for (uint8_t i = 0; i < 5; ++i) {
                if (!picked[i]) totalWeight += weights[i];
            }
            if (totalWeight == 0) break;
            uint16_t roll = (uint16_t)random(totalWeight);
            uint8_t chosen = 0;
            for (uint8_t i = 0; i < 5; ++i) {
                if (picked[i]) continue;
                if (roll < weights[i]) {
                    chosen = i;
                    break;
                }
                roll -= weights[i];
            }
            picked[chosen] = true;
            pickedAttrs[slot] = chosen;
        }

        uint16_t shareWeights[5] = {0, 0, 0, 0, 0};
        uint16_t shareTotal = 0;
        for (uint8_t slot = 0; slot < attrSlots; ++slot) {
            uint16_t share = (uint16_t)random(70, 131);
            shareWeights[slot] = share;
            shareTotal += share;
        }
        if (shareTotal == 0) return;

        float bonuses[5] = {0, 0, 0, 0, 0};
        for (uint8_t slot = 0; slot < attrSlots; ++slot) {
            bonuses[pickedAttrs[slot]] += rewardTotal * (float)shareWeights[slot] / (float)shareTotal;
        }

        bug.addTrainingBonus(bonuses[0], bonuses[1], bonuses[2], bonuses[3], bonuses[4]);
        uint8_t motBonus = 0;
        if (includeMot) {
            motBonus = (uint8_t)random(3, 9);
            uint16_t mot = (uint16_t)bug.getMot() + motBonus;
            bug.setMot(mot > 100 ? 100 : (uint8_t)mot);
        }
        forceSave();
        Serial.printf("[Engine] Visit rewards eat=%u play=%u elapsed=%lu total=%.2f "
                      "siz=%.2f str=%.2f end=%.2f spd=%.2f spi=%.2f mot=%u\n",
                      eatCount, playCount, (unsigned long)elapsedMs, rewardTotal,
                      bonuses[0], bonuses[1], bonuses[2], bonuses[3], bonuses[4], motBonus);
    }
    PendingNpcBattle npcBattle;
    NpcBattleResult npcResult;

    // 杯赛全局数据（跨虫持久）
    uint16_t cupSeason = 0;
    uint32_t lastCupGameTime = 0; // 上一届杯赛开始时的游戏时间（秒）
    CupCycleState cupCycleState = CupCycleState::IDLE;

    static constexpr uint32_t ACTIVE_FRAME_MS  = 50;    // ~20fps
    static constexpr uint32_t IDLE_FRAME_MS    = 100;   // 10fps
    static constexpr uint32_t VISIT_HOST_RENDER_MS = ACTIVE_FRAME_MS; // Visit 时保持 20fps
    static constexpr uint32_t INPUT_SAMPLE_MS  = 16;    // 输入采样 60Hz
    static constexpr uint32_t AUTO_SAVE_MS     = 60000; // 60s 自动保存
    static constexpr uint32_t LIGHT_SLEEP_MS   = 60000; // 1min 后进入 Deep Sleep
    static constexpr uint32_t IMU_SAMPLE_MS    = 50;    // IMU 采样 20Hz

    uint32_t lastImuTime = 0;
    uint32_t lastRenderTime = 0;

    void processInput();
    bool routeButtonEvent(const ButtonEvent& ev);
    bool handleGlobalButtonEvent(const ButtonEvent& ev);
    void processIMU();
    void checkCupCycle();
    uint8_t naturalExploreTimeOfDay() const;
    void saveExploreGlobal();
    uint32_t targetFrameTime() const;
    void resetIdleTimer();
};
