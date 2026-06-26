#include "GameEngine.h"
#include "UiStrings.h"
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
#include "../assets/BowlAssets.h"
#include "../hardware/BattleLink.h"

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
        SaveManager::ins().clearTerrariumViewState();
    } else {
        gameNow = bug.getLastUpdateTime();
        Serial.printf("[Engine] Bug loaded, stage=%d alive=%d gameNow=%llu\n",
                      (int)bug.getStage(), bug.isAlive(), gameNow);
        if (bug.getStage() == Stage::ADULT && bug.isAlive()) {
            SaveManager::ins().loadTerrariumViewState(terrariumViewState);
        } else {
            terrariumViewState = TerrariumViewState();
            SaveManager::ins().clearTerrariumViewState();
        }
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
    uint8_t loadedToy = TOY_NONE;
    if (SaveManager::ins().loadSettings(loadedFont, loadedBri, loadedSpeed, loadedIdle,
                                        loadedBg, loadedWood, loadedBowl, loadedFood,
                                        loadedToy)) {
        setFontScale(loadedFont);
        brightness = loadedBri;
        gameSpeed = loadedSpeed;
        idleTimeoutIndex = loadedIdle;
        setMainSceneBg(loadedBg);
        setWoodStyle(loadedWood);
        setBowlStyle(loadedBowl);
        setFoodStyle(loadedFood);
        setToyStyle(loadedToy);
        bug.setFoodTray(bowlStyle == 0xFF ? 0 : bowlStyle + 1, (FoodType)foodStyle);
        bug.setWood(woodStyle);
        Hal::ins().setBrightness(brightness);
        Serial.printf("[Engine] Settings loaded: font=%.2f bri=%d speed=%.1f idle=%d bg=%d wood=%d bowl=%d food=%d toy=%d\n",
                      fontScale, brightness, gameSpeed, idleTimeoutIndex,
                      mainSceneBg, woodStyle, bowlStyle, foodStyle, toyStyle);
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

    BattleLink::ins().setVisitPowerSave(hasActiveVisitSession());

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
            uint32_t renderInterval = (curSceneID == SCENE_TERRARIUM && isVisitHost())
                                      ? VISIT_HOST_RENDER_MS
                                      : target;
            if (lastRenderTime == 0 || realNow - lastRenderTime >= renderInterval) {
                curScene->render();
                Hal::ins().flush();
                lastRenderTime = realNow;
            }
        } else if (!idleRendered) {
            curScene->render();
            Hal::ins().flush();
            idleRendered = true;
            lastRenderTime = realNow;
        }
    }

    // Deep Sleep 入口
    if (systemState == SystemState::DEEP_SLEEP) {
        forceSave();
        SaveManager::ins().saveSettings(PixelRenderer::getContentFontScale(), brightness,
                                        gameSpeed, idleTimeoutIndex, mainSceneBg,
                                        woodStyle, bowlStyle, foodStyle, toyStyle);
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
        curScene->onExit();
        delete curScene;
        curScene = nullptr;
    }

    resetIdleTimer();
    lastRenderTime = 0;
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
    }
}

void GameEngine::processInput() {
    Hal& hal = Hal::ins();
    bool rawA = hal.btnA_raw();
    bool rawB = hal.btnB_raw();

    ButtonEvent events[ButtonDispatcher::MAX_EVENTS_PER_POLL];
    uint8_t eventCount = ButtonDispatcher::ins().poll(events, ButtonDispatcher::MAX_EVENTS_PER_POLL);
    for (uint8_t i = 0; i < eventCount; ++i) {
        routeButtonEvent(events[i]);
    }

    if (systemState != SystemState::DEEP_SLEEP && (rawA || rawB)) {
        systemState = SystemState::ACTIVE;
        idleTimer = 0;
        idleRendered = false;
    }
}

bool GameEngine::routeButtonEvent(const ButtonEvent& ev) {
    if (curScene && curScene->onButton(ev)) {
        return true;
    }
    return handleGlobalButtonEvent(ev);
}

bool GameEngine::handleGlobalButtonEvent(const ButtonEvent& ev) {
    if (ev.btn == 1 && ev.action == BtnAction::LONG_PRESS && curSceneID == SCENE_TERRARIUM) {
        systemState = SystemState::DEEP_SLEEP;
        Serial.println("[Engine] Deep sleep triggered by long-press B");
        return true;
    }
    return false;
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


#include "game_engine/GameEngineCupExploreSleep.inc"
