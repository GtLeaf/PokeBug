#include "GameEngine.h"
#include "esp_sleep.h"
#include "../scenes/TerrariumScene.h"
#include "../scenes/MenuScene.h"
#include "../scenes/InfoScene.h"
#include "../scenes/SettingsScene.h"
#include "../scenes/BattleScene.h"

GameEngine& GameEngine::ins() {
    static GameEngine instance;
    return instance;
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
    if (SaveManager::ins().loadSettings(loadedFont, loadedBri, loadedSpeed, loadedIdle)) {
        fontScale = loadedFont;
        brightness = loadedBri;
        gameSpeed = loadedSpeed;
        idleTimeoutIndex = loadedIdle;
        PixelRenderer::setContentFontScale(fontScale);
        Hal::ins().setBrightness(brightness);
        Serial.printf("[Engine] Settings loaded: font=%.2f bri=%d speed=%.1f idle=%d\n",
                      fontScale, brightness, gameSpeed, idleTimeoutIndex);
    } else {
        PixelRenderer::setContentFontScale(fontScale);
    }

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

    // 更新虚拟时间与独角仙
    uint32_t virtualElapsed = (uint32_t)(realElapsed * gameSpeed);
    gameNow += virtualElapsed;
    bug.update(gameNow);

    // 更新场景逻辑
    if (curScene) {
        SceneID next = curScene->update();
        if (next != SCENE_NONE) {
            Serial.printf("[Engine] Scene switch: %d -> %d\n", curSceneID, next);
            switchScene(next);
        }
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
        SaveManager::ins().saveSettings(PixelRenderer::getContentFontScale(), brightness, gameSpeed, idleTimeoutIndex);
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
        case SCENE_BATTLE:
            curScene = new BattleScene();
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
    Hal::ins().updateIMU();
    if (Hal::ins().isShaken()) {
        if (bug.onShake(gameNow)) {
            Serial.println("[Engine] Shake detected and processed");
            resetIdleTimer();
        }
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

uint32_t GameEngine::getIdleTimeoutMs() const {
    // 0 = never
    static const uint32_t TIMEOUT_MS[5] = { 30000, 60000, 120000, 300000, 0 };
    return TIMEOUT_MS[idleTimeoutIndex];
}
