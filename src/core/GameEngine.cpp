#include "GameEngine.h"
#include "esp_sleep.h"
#include "../scenes/TerrariumScene.h"
#include "../scenes/MenuScene.h"
#include "../scenes/InfoScene.h"
#include "../scenes/SettingsScene.h"
#include "../scenes/LobbyScene.h"
#include "../scenes/BattleScene.h"
#include "../scenes/ExploreScene.h"
#include "../scenes/CupScene.h"
#include "../game/ItemCatalog.h"

GameEngine& GameEngine::ins() {
    static GameEngine instance;
    return instance;
}

namespace {
uint32_t exploreCycleDayFromMs(uint64_t gameNowMs) {
    static constexpr uint64_t HOUR_MS = 60ULL * 60 * 1000;
    uint32_t calendarDay = (uint32_t)(gameNowMs / GameEngine::GAME_DAY_MS);
    uint8_t hour = (uint8_t)((gameNowMs % GameEngine::GAME_DAY_MS) / HOUR_MS);

    // 探索日从早晨 05:00 开始。午夜到清晨仍属于前一轮夜晚，避免夜探结束后立刻刷新次数。
    if (hour < 5 && calendarDay > 0) {
        return calendarDay - 1;
    }
    return calendarDay;
}
}

void GameEngine::begin() {
    Hal::ins().begin();

    // 尝试加载存档
    if (!SaveManager::ins().load(bug)) {
        Serial.println("[Engine] No save found, creating new bug");
        bug.initNew(0);
        gameNow = 0;
    } else {
        gameNow = bug.getLastUpdateTime();
        Serial.printf("[Engine] Bug loaded, stage=%d alive=%d gameNow=%llu\n",
                      (int)bug.getStage(), bug.isAlive(), gameNow);
    }

    // 加载设置
    float loadedFont = 1.5f;
    uint8_t loadedBri = 128;
    float loadedSpeed = 1.0f;
    uint8_t loadedIdle = 0;  // 默认 30s
    uint8_t loadedBg = BG_MOSS;
    uint8_t loadedWood = 0;
    uint8_t loadedBowl = 0;
    uint8_t loadedFood = 0;
    if (SaveManager::ins().loadSettings(loadedFont, loadedBri, loadedSpeed, loadedIdle,
                                        loadedBg, loadedWood, loadedBowl, loadedFood)) {
        fontScale = loadedFont;
        brightness = loadedBri;
        gameSpeed = loadedSpeed;
        idleTimeoutIndex = loadedIdle;
        setMainSceneBg(loadedBg);
        setWoodStyle(loadedWood);
        setBowlStyle(loadedBowl);
        setFoodStyle(loadedFood);
        bug.setFoodTray(bowlStyle + 1, (FoodType)foodStyle);
        bug.setWood(woodStyle);
        PixelRenderer::setContentFontScale(fontScale);
        Hal::ins().setBrightness(brightness);
        Serial.printf("[Engine] Settings loaded: font=%.2f bri=%d speed=%.1f idle=%d bg=%d wood=%d bowl=%d food=%d\n",
                      fontScale, brightness, gameSpeed, idleTimeoutIndex,
                      mainSceneBg, woodStyle, bowlStyle, foodStyle);
    } else {
        PixelRenderer::setContentFontScale(fontScale);
    }

    // 加载全局杯赛数据
    uint8_t cupStateRaw = 0;
    SaveManager::ins().loadCupGlobal(cupSeason, lastCupGameTime, cupStateRaw);
    if (cupStateRaw <= (uint8_t)CupCycleState::IN_PROGRESS) {
        cupCycleState = static_cast<CupCycleState>(cupStateRaw);
    } else {
        cupCycleState = CupCycleState::IDLE;
    }
    Serial.printf("[Engine] Cup global loaded: season=%u lastGameTime=%u state=%u gameNow=%llu\n",
                  cupSeason, lastCupGameTime, cupStateRaw, gameNow);

    uint8_t loadedExploreTod = TIME_MORNING;
    uint8_t loadedExploreCount = 0;
    uint32_t loadedExploreDay = exploreCycleDayFromMs(gameNow);
    if (SaveManager::ins().loadExploreGlobal(loadedExploreDay, loadedExploreTod, loadedExploreCount)) {
        exploreDay = loadedExploreDay;
        timeOfDay = loadedExploreTod <= TIME_EVENING ? loadedExploreTod : TIME_MORNING;
        exploreCountToday = loadedExploreCount > EXPLORE_DAILY_LIMIT ? EXPLORE_DAILY_LIMIT : loadedExploreCount;
    } else {
        exploreDay = exploreCycleDayFromMs(gameNow);
        timeOfDay = naturalExploreTimeOfDay();
        exploreCountToday = 0;
    }
    syncExploreClock(false);
    Serial.printf("[Engine] Explore global loaded: day=%u tod=%u count=%u\n",
                  exploreDay, timeOfDay, exploreCountToday);

    // 首次迁移或新存档：用当前游戏时间作为上一届起点，避免旧真实时间秒被误用
    if (lastCupGameTime == 0 || (uint64_t)lastCupGameTime * 1000ULL > gameNow) {
        lastCupGameTime = (uint32_t)(gameNow / 1000ULL);
        Serial.printf("[Engine] Cup time reset to current game time\n");
    }

    // 新存档赛季从 1 开始显示
    if (cupSeason == 0) {
        cupSeason = 1;
    }

    // 根据当前游戏时间立即校正杯赛周期状态
    checkCupCycle();

    // 注册全局按钮监听：仅在培养缸主场景时长按 B 进入 Deep Sleep
    engineDispatcherHandle = ButtonDispatcher::ins().subscribe([this](const ButtonEvent& ev) -> bool {
        if (ev.btn == 1 && ev.action == BtnAction::LONG_PRESS && curSceneID == SCENE_TERRARIUM) {
            systemState = SystemState::DEEP_SLEEP;
            Serial.println("[Engine] Deep sleep triggered by long-press B");
            return true;
        }
        return false;
    }, 100);

    switchScene(SCENE_TERRARIUM);
}

void GameEngine::run() {
    uint32_t realNow = Hal::ins().millis();

    if (lastFrameTime == 0) {
        lastFrameTime = realNow;
        prevRealNow = realNow;
        lastInputTime = realNow;
        lastImuTime = realNow;
        Serial.printf("[Engine] First frame, realNow=%u gameNow=%llu speed=%.1f\n",
                      realNow, gameNow, gameSpeed);
    }

    // 输入采样
    if (realNow - lastInputTime >= INPUT_SAMPLE_MS) {
        M5.update();
        processInput();
        lastInputTime = realNow;
    }

    // IMU 采样
    if (realNow - lastImuTime >= IMU_SAMPLE_MS) {
        processIMU();
        lastImuTime = realNow;
    }

    // 帧率控制
    uint32_t frameElapsed = realNow - lastFrameTime;
    uint32_t target = targetFrameTime();

    if (frameElapsed < target) {
        delay(1);
        return;
    }

    uint32_t realElapsed = realNow - prevRealNow;
    prevRealNow = realNow;

    if (frameElapsed > target * 3) {
        Serial.printf("[Engine] SKIP frameElapsed=%u > 3*target, reset lastFrameTime\n", frameElapsed);
        lastFrameTime = realNow;
    } else {
        lastFrameTime += target;
    }

    idleTimer += frameElapsed;

    // 更新虚拟时间与独角仙。菜单/探索/Cup 等流程暂停游戏时间，
    // 避免玩家停留在流程中时继续结算饥饿、成长和报名窗口。
    bool gameTimePaused = isGameTimePausedScene();
    if (!gameTimePaused) {
        uint32_t virtualElapsed = (uint32_t)(realElapsed * gameSpeed);
        gameNow += virtualElapsed;
        bug.update(gameNow);
        syncExploreClock(true);
    }

    // 更新场景逻辑
    if (curScene) {
        SceneID next = curScene->update();
        if (next != SCENE_NONE) {
            Serial.printf("[Engine] Scene switch: %d -> %d\n", curSceneID, next);
            switchScene(next);
        }
    }

    // 杯赛周期检查（基于虚拟游戏时间）
    if (!gameTimePaused) {
        checkCupCycle();
    }

    // 自动保存
    if (realNow - lastSaveTime > AUTO_SAVE_MS) {
        forceSave();
        lastSaveTime = realNow;
    }

    // 状态转换（timeout=0 表示 never）
    uint32_t idleTimeout = getIdleTimeoutMs();
    if (systemState == SystemState::ACTIVE && idleTimeout > 0 && idleTimer > idleTimeout) {
        systemState = SystemState::IDLE;
    }
    // 仅在培养缸主场景下才进入 Deep Sleep
    if (systemState == SystemState::IDLE && idleTimer > LIGHT_SLEEP_MS && curSceneID == SCENE_TERRARIUM) {
        systemState = SystemState::DEEP_SLEEP;
    }

    Hal::ins().setIdleBrightness(systemState != SystemState::ACTIVE && curSceneID == SCENE_TERRARIUM);

    // 渲染
    if (curScene) {
        if (systemState == SystemState::ACTIVE) {
            curScene->render();
            Hal::ins().flush();
        } else if (!idleRendered) {
            curScene->render();
            Hal::ins().flush();
            idleRendered = true;
        }
    }

    // Deep Sleep 入口
    if (systemState == SystemState::DEEP_SLEEP) {
        forceSave();
        SaveManager::ins().saveSettings(PixelRenderer::getContentFontScale(), brightness,
                                        gameSpeed, idleTimeoutIndex, mainSceneBg,
                                        woodStyle, bowlStyle, foodStyle);
        SaveManager::ins().saveCupGlobal(cupSeason, lastCupGameTime, (uint8_t)cupCycleState);
        Serial.println("[Engine] Enter deep sleep");
        Hal::ins().setBrightness(0);
        esp_sleep_enable_timer_wakeup(600 * 1000000ULL);
        esp_deep_sleep_start();
    }

    frames++;
}

void GameEngine::switchScene(SceneID id) {
    prevSceneID = curSceneID;

    if (curScene) {
        if (sceneDispatcherHandle >= 0) {
            ButtonDispatcher::ins().unsubscribe(sceneDispatcherHandle);
            sceneDispatcherHandle = -1;
        }
        curScene->onExit();
        delete curScene;
        curScene = nullptr;
    }

    resetIdleTimer();
    curSceneID = id;

    switch (id) {
        case SCENE_TERRARIUM:
            curScene = new TerrariumScene();
            break;
        case SCENE_MENU:
            curScene = new MenuScene();
            break;
        case SCENE_INFO:
            curScene = new InfoScene();
            break;
        case SCENE_SETTINGS:
            curScene = new SettingsScene();
            break;
        case SCENE_LOBBY:
            curScene = new LobbyScene();
            break;
        case SCENE_BATTLE:
            curScene = new BattleScene();
            break;
        case SCENE_EXPLORE:
            curScene = new ExploreScene();
            break;
        case SCENE_CUP:
            curScene = new CupScene();
            break;
        default:
            curScene = new TerrariumScene();
            break;
    }

    if (curScene) {
        curScene->onEnter();
        sceneDispatcherHandle = ButtonDispatcher::ins().subscribe([this](const ButtonEvent& ev) -> bool {
            if (this->curScene) {
                return this->curScene->onButton(ev);
            }
            return false;
        }, 0);
    }
}

void GameEngine::processInput() {
    Hal& hal = Hal::ins();
    bool rawA = hal.btnA_raw();
    bool rawB = hal.btnB_raw();

    ButtonDispatcher::ins().poll();

    if (systemState != SystemState::DEEP_SLEEP && (rawA || rawB)) {
        systemState = SystemState::ACTIVE;
        idleTimer = 0;
        idleRendered = false;
    }
}

void GameEngine::processIMU() {
    Hal& hal = Hal::ins();
    hal.updateIMU();

    uint32_t realNow = hal.millis();
    uint32_t delta = realNow - lastImuTime;

    // 卵期倾斜记录：用于气质判定
    if (bug.getStage() == Stage::EGG && delta > 0) {
        float ax, ay, az;
        hal.getAccel(ax, ay, az);
        const float TILT_THRESHOLD = 0.3f; // g
        if (ax > TILT_THRESHOLD) {
            bug.onEggTilt(gameNow, delta, false);
        } else if (ax < -TILT_THRESHOLD) {
            bug.onEggTilt(gameNow, delta, true);
        }
    }

    // 探索场景不再使用倾斜控制；这里仅保留全局摇动检测
    if (hal.isShaken()) {
        if (bug.onShake(gameNow)) {
            Serial.println("[Engine] Shake detected and processed");
            resetIdleTimer();
        }
    }
}

void GameEngine::checkCupCycle() {
    // 深睡等场景可能一次性推进多个 7 天周期，用循环对齐到当前周期
    while (gameNow >= (uint64_t)lastCupGameTime * 1000ULL + CUP_CYCLE_MS) {
        lastCupGameTime += (uint32_t)(CUP_CYCLE_MS / 1000ULL);
        cupSeason++;
        cupCycleState = CupCycleState::IDLE;
        Serial.printf("[Engine] Cup season advanced to %u, startTime=%u\n", cupSeason, lastCupGameTime);
    }

    uint64_t registerStart = (uint64_t)lastCupGameTime * 1000ULL;
    uint64_t registerEnd   = registerStart + CUP_REGISTER_MS;

    switch (cupCycleState) {
        case CupCycleState::IDLE:
            if (gameNow >= registerEnd) {
                // 已过报名截止时间，本赛季自动关闭
                cupCycleState = CupCycleState::REGISTER_EXPIRED;
                Serial.printf("[Engine] Cup registration expired (reboot gap)\n");
            } else if (gameNow >= registerStart) {
                // 报名期开始
                cupCycleState = CupCycleState::REGISTER_OPEN;
                Serial.printf("[Engine] Cup registration open until gameNow=%llu\n", registerEnd);
            }
            break;

        case CupCycleState::REGISTER_OPEN:
            if (gameNow >= registerEnd) {
                // 报名结束，玩家未参与
                cupCycleState = CupCycleState::REGISTER_EXPIRED;
                Serial.printf("[Engine] Cup registration expired\n");
            }
            break;

        case CupCycleState::REGISTER_EXPIRED:
        case CupCycleState::IN_PROGRESS:
        default:
            break;
    }
}

bool GameEngine::isBlockDeepSleepScene() const {
    return curSceneID == SCENE_BATTLE ||
           curSceneID == SCENE_LOBBY ||
           curSceneID == SCENE_EXPLORE ||
           curSceneID == SCENE_CUP ||
           curSceneID == SCENE_MENU;
}

bool GameEngine::isGameTimePausedScene() const {
    switch (curSceneID) {
        case SCENE_MENU:
        case SCENE_INFO:
        case SCENE_SETTINGS:
        case SCENE_LOBBY:
        case SCENE_EXPLORE:
        case SCENE_CUP:
        case SCENE_BATTLE:
            return true;
        default:
            return false;
    }
}

uint32_t GameEngine::targetFrameTime() const {
    if (systemState != SystemState::ACTIVE) {
        return IDLE_FRAME_MS;
    }
    return ACTIVE_FRAME_MS;
}

void GameEngine::forceSave() {
    SaveManager::ins().save(bug);
    saveExploreGlobal();
}

void GameEngine::resetIdleTimer() {
    idleTimer = 0;
    lastFrameTime = Hal::ins().millis();
    systemState = SystemState::ACTIVE;
}

void GameEngine::setIdleTimeoutIndex(uint8_t idx) {
    if (idx > 4) idx = 4;
    idleTimeoutIndex = idx;
}

void GameEngine::setMainSceneBg(uint8_t id) {
    if (id >= BG_COUNT) id = BG_MOSS;
    mainSceneBg = id;
}

void GameEngine::cycleMainSceneBg() {
    mainSceneBg++;
    if (mainSceneBg >= BG_COUNT) mainSceneBg = BG_MOSS;
}

const char* GameEngine::getMainSceneBgName() const {
    switch (mainSceneBg) {
        case BG_BEGINNER: return "Girl";
        case BG_CHILD_ROOM: return "Boy";
        case BG_ENTOMOLOGIST: return "Lab";
        case BG_SCHOOL: return "School";
        case BG_MOSS:
        default:
            return "Room";
    }
}

void GameEngine::setWoodStyle(uint8_t id) {
    if (id >= WoodTypeInfo::COUNT) id = 0;
    woodStyle = id;
}

void GameEngine::cycleWoodStyle() {
    woodStyle++;
    if (woodStyle >= WoodTypeInfo::COUNT) woodStyle = 0;
}

const char* GameEngine::getWoodStyleName() const {
    return WoodTypeInfo::name((WoodType)woodStyle);
}

void GameEngine::setBowlStyle(uint8_t id) {
    if (id >= 3) id = 0;
    uint8_t wins = getBug().getWins();
    // Lv.2 需 2 胜，Lv.3 需 5 胜
    if (id >= 2 && wins < 5) id = 1;
    if (id >= 1 && wins < 2) id = 0;
    bowlStyle = id;
}

void GameEngine::cycleBowlStyle() {
    uint8_t start = bowlStyle;
    do {
        bowlStyle++;
        if (bowlStyle >= 3) bowlStyle = 0;
    } while (bowlStyle != start && !isBowlStyleUnlocked(bowlStyle));
}

bool GameEngine::isBowlStyleUnlocked(uint8_t id) const {
    uint8_t wins = getBug().getWins();
    if (id == 0) return true;
    if (id == 1) return wins >= 2;
    if (id == 2) return wins >= 5;
    return false;
}

const char* GameEngine::getBowlStyleName() const {
    switch (bowlStyle) {
        case 1: return "Block";
        case 2: return "Root";
        case 0:
        default:
            return "Low";
    }
}

void GameEngine::setFoodStyle(uint8_t id) {
    if (id >= 6) id = 0;
    foodStyle = id;
}

void GameEngine::cycleFoodStyle() {
    foodStyle++;
    if (foodStyle >= 6) foodStyle = 0;
}

const char* GameEngine::getFoodStyleName() const {
    switch (foodStyle) {
        case 1: return "Cube";
        case 2: return "Slice";
        case 3: return "Citrus";
        case 4: return "Jelly";
        case 5: return "Berry";
        case 0:
        default:
            return "Drop";
    }
}

void GameEngine::setExploreLocation(uint8_t id) {
    if (id >= EXPLORE_LOCATION_COUNT) id = EXPLORE_PARK;
    exploreLocation = id;
}

const char* GameEngine::getExploreLocationName() const {
    switch (exploreLocation) {
        case EXPLORE_BACK_HILL: return "Back Hill";
        case EXPLORE_RIVERSIDE: return "Riverside";
        case EXPLORE_OLD_WOODS: return "Old Woods";
        case EXPLORE_PARK:
        default:
            return "Park";
    }
}

const char* GameEngine::getTimeOfDayName() const {
    switch (timeOfDay) {
        case TIME_AFTERNOON: return "Afternoon";
        case TIME_EVENING: return "Evening";
        case TIME_MORNING:
        default:
            return "Morning";
    }
}

const char* GameEngine::getTimeOfDayShortName() const {
    switch (timeOfDay) {
        case TIME_AFTERNOON: return "AFT";
        case TIME_EVENING: return "EVE";
        case TIME_MORNING:
        default:
            return "MOR";
    }
}

void GameEngine::getExploreClockText(char* buf, size_t bufSize) const {
    if (!buf || bufSize == 0) return;
    static constexpr uint64_t HOUR_MS = 60ULL * 60 * 1000;
    static constexpr uint64_t DAY_MS = 24ULL * HOUR_MS;
    uint64_t msInDay = gameNow % DAY_MS;
    uint8_t hour = (uint8_t)(msInDay / HOUR_MS);
    uint8_t minute = (uint8_t)((msInDay % HOUR_MS) / (60ULL * 1000));
    snprintf(buf, bufSize, "%02u:%02u", hour, minute);
}

uint32_t GameEngine::getCurrentGameDay() const {
    return (uint32_t)(gameNow / GAME_DAY_MS);
}

uint8_t GameEngine::naturalExploreTimeOfDay() const {
    return naturalExploreTimeOfDayFromMs(gameNow);
}

uint8_t GameEngine::naturalExploreTimeOfDayFromMs(uint64_t gameNowMs) {
    static constexpr uint64_t HOUR_MS = 60ULL * 60 * 1000;
    uint64_t hour = (gameNowMs % GAME_DAY_MS) / HOUR_MS;
    // 05:00-11:59 早晨，12:00-18:59 下午，19:00-04:59 夜晚
    if (hour >= 5 && hour < 12) return TIME_MORNING;
    if (hour >= 12 && hour < 19) return TIME_AFTERNOON;
    return TIME_EVENING;
}

bool GameEngine::isExploreTimeAllowedFromMs(uint64_t gameNowMs) {
    static constexpr uint64_t HOUR_MS = 60ULL * 60 * 1000;
    uint64_t hour = (gameNowMs % GAME_DAY_MS) / HOUR_MS;
    return hour >= 5;
}

bool GameEngine::isExploreTimeAllowed() const {
    return isExploreTimeAllowedFromMs(gameNow);
}

void GameEngine::saveExploreGlobal() {
    SaveManager::ins().saveExploreGlobal(exploreDay, timeOfDay, exploreCountToday);
}

void GameEngine::syncExploreClock(bool persist) {
    uint32_t gameDay = exploreCycleDayFromMs(gameNow);
    uint8_t naturalTod = naturalExploreTimeOfDay();
    bool changed = false;

    if (gameDay > exploreDay) {
        // 进入新的探索日：重置次数，时段按自然时间
        exploreDay = gameDay;
        exploreCountToday = 0;
        timeOfDay = naturalTod;
        changed = true;
    } else if (gameDay < exploreDay) {
        // 游戏时间被重置（如新虫、手动重置），修正全局探索时钟
        exploreDay = gameDay;
        exploreCountToday = 0;
        timeOfDay = naturalTod;
        changed = true;
    } else if (timeOfDay < naturalTod) {
        // 同一天内自然推进时段
        timeOfDay = naturalTod;
        changed = true;
    }

    if (timeOfDay > TIME_EVENING) {
        timeOfDay = TIME_MORNING;
        changed = true;
    }
    if (exploreCountToday > EXPLORE_DAILY_LIMIT) {
        exploreCountToday = EXPLORE_DAILY_LIMIT;
        changed = true;
    }

    if (changed && persist) {
        saveExploreGlobal();
    }
}

bool GameEngine::canExplore() const {
    if (bug.getStage() != Stage::ADULT) return false;
    if (bug.isDead()) return false;
    if (bug.getHunger() < 30) return false;
    if (bug.getMot() < 50) return false;
    if (!isExploreTimeAllowed()) return false;
    return isExploreLimitBypassed() || exploreCountToday < EXPLORE_DAILY_LIMIT;
}

bool GameEngine::isExploreLimitBypassed() {
#ifdef POKEBUG_TEST_UNLOCK_EXPLORE
    return true;
#else
    return false;
#endif
}

void GameEngine::recordExploreFinished() {
    if (exploreCountToday < EXPLORE_DAILY_LIMIT) exploreCountToday++;
    syncExploreClock(false);
    forceSave();
}

uint32_t GameEngine::getIdleTimeoutMs() const {
    // 0 = never
    static const uint32_t TIMEOUT_MS[5] = { 30000, 60000, 120000, 300000, 0 };
    return TIMEOUT_MS[idleTimeoutIndex];
}

void GameEngine::setPendingNpcBattle(const NpcCombatant& npc, SceneID returnScene, bool legend, bool fromExplore, bool fromCup) {
    npcBattle.active = true;
    npcBattle.returnScene = returnScene;
    npcBattle.siz = npc.siz;
    npcBattle.str = npc.str;
    npcBattle.end = npc.end;
    npcBattle.spd = npc.spd;
    npcBattle.spi = npc.spi;
    npcBattle.mot = npc.mot;
    npcBattle.palette = npc.palette;
    npcBattle.tier = npc.tier;
    npcBattle.legend = legend;
    npcBattle.fromExplore = fromExplore;
    npcBattle.fromCup = fromCup;
    npcResult.valid = false;
    npcResult.won = false;
    npcResult.tier = npc.tier;
    npcResult.legend = legend;
    npcResult.fromExplore = fromExplore;
    npcResult.fromCup = fromCup;
    npcResult.spiBoosted = false;
}
