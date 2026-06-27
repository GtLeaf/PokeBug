#include <Arduino.h>
#include "esp_sleep.h"
#include "core/GameEngine.h"
#include "core/SaveManager.h"
#include "hardware/Hal.h"
#include "hardware/PixelRenderer.h"
#include "game/Bug.h"

void setup() {
    Serial.begin(115200);
    delay(100);

    // Deep Sleep 定时器唤醒：最小化更新路径
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause == ESP_SLEEP_WAKEUP_TIMER) {
        Serial.println("[Boot] Deep sleep timer wakeup");

        Bug bug;
        if (!SaveManager::ins().load(bug)) {
            bug.initNew(0);
        }

        float fontScale = 1.5f;
        uint8_t brightness = 128;
        float gameSpeed = 1.0f;
        uint8_t idleTimeout = 0;
        uint8_t mainSceneBg = 0;
        uint8_t woodStyle = 0;
        uint8_t bowlStyle = 0;
        uint8_t foodStyle = 0;
        uint8_t toyStyle = GameEngine::TOY_NONE;
        SaveManager::ins().loadSettings(fontScale, brightness, gameSpeed, idleTimeout,
                                        mainSceneBg, woodStyle, bowlStyle, foodStyle,
                                        toyStyle);

        // 推进虚拟时间 10 分钟并更新。设备深睡时，甲虫也按睡眠状态结算。
        uint64_t gameNow = bug.getLastUpdateTime() + (uint64_t)(600000 * gameSpeed);
        Stage prevStage = bug.getStage();
        bug.ensureMinHunger(1);
        bug.setSleeping(true);
        bug.update(gameNow);
        bug.setSleeping(false);
        if (prevStage == Stage::PUPA && bug.getStage() != Stage::PUPA) {
            if (bowlStyle == 0xFF || bowlStyle >= 3) bowlStyle = 0;
            GameEngine::prepareJuvenileStarterTray(bug, bowlStyle);
            Serial.printf("[Boot] Pupa hatched during deep sleep: bowlStyle=%u\n", bowlStyle);
        }

        uint32_t exploreDay = (uint32_t)(gameNow / GameEngine::GAME_DAY_MS);
        uint8_t exploreTod = GameEngine::TIME_MORNING;
        uint8_t exploreCount = 0;
        SaveManager::ins().loadExploreGlobal(exploreDay, exploreTod, exploreCount);
        uint32_t gameDay = (uint32_t)(gameNow / GameEngine::GAME_DAY_MS);
        uint8_t naturalTod = GameEngine::naturalExploreTimeOfDayFromMs(gameNow);
        if (gameDay > exploreDay || exploreDay > gameDay) {
            // 新游戏日或游戏时间被重置：修正探索时钟
            exploreDay = gameDay;
            exploreCount = 0;
            exploreTod = naturalTod;
        } else if (gameDay == exploreDay && exploreTod < naturalTod) {
            exploreTod = naturalTod;
        }
        if (exploreTod > GameEngine::TIME_EVENING) exploreTod = GameEngine::TIME_MORNING;
        if (exploreCount > GameEngine::EXPLORE_DAILY_LIMIT) {
            exploreCount = GameEngine::EXPLORE_DAILY_LIMIT;
        }

        SaveManager::ins().save(bug);
        SaveManager::ins().saveExploreGlobal(exploreDay, exploreTod, exploreCount);
        SaveManager::ins().saveSettings(fontScale, brightness, gameSpeed, idleTimeout,
                                        mainSceneBg, woodStyle, bowlStyle, foodStyle,
                                        toyStyle);
        Serial.println("[Boot] Bug updated, re-entering deep sleep");

        esp_sleep_enable_timer_wakeup(600 * 1000000ULL);
        esp_deep_sleep_start();
    }

    // 正常上电/复位路径
    Serial.println("[Boot] Starting PokeBug...");

    randomSeed(esp_random());

    GameEngine::ins().begin();
    Serial.printf("[Boot] PSRAM found=%d size=%u free=%u\n",
                  psramFound() ? 1 : 0,
                  ESP.getPsramSize(),
                  ESP.getFreePsram());
    PixelRenderer::bind(&Hal::ins().canvas());

    Serial.println("[Boot] Init done");
}

void loop() {
    GameEngine::ins().run();
}
