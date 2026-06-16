#include "SettingsScene.h"
#include "../core/GameEngine.h"
#include "../core/SaveManager.h"

void SettingsScene::onEnter() {
    cursor = 0;
    confirmReset = false;
}

void SettingsScene::onExit() {
    saveSettingsNow();
}

bool SettingsScene::onButton(const ButtonEvent& ev) {
    if (confirmReset) {
        if (ev.btn == 0 && ev.action == BtnAction::PRESSED) {
            SaveManager::ins().clear();
            GameEngine::ins().getBug().initNew(0);
            GameEngine::ins().resetGameNow();
            GameEngine::ins().setGameSpeed(1.0f);
            GameEngine::ins().setIdleTimeoutIndex(0);
            GameEngine::ins().setFontScale(1.5f);
            Hal::ins().setBrightness(128);
            confirmReset = false;
            nextScene = SCENE_TERRARIUM;
            return true;
        }
        if ((ev.btn == 1 && ev.action == BtnAction::PRESSED) ||
            (ev.action == BtnAction::LONG_PRESS)) {
            confirmReset = false;
            return true;
        }
        return false;
    }

    // 任何子菜单长按都返回培养缸
    if (ev.action == BtnAction::LONG_PRESS) {
        nextScene = SCENE_TERRARIUM;
        return true;
    }

    if (ev.btn == 1 && ev.action == BtnAction::PRESSED) {
        cursor++;
        if (cursor >= ITEM_COUNT) cursor = 0;
        return true;
    }

    if (ev.btn == 0 && ev.action == BtnAction::PRESSED) {
        switch (cursor) {
            case ITEM_BRIGHTNESS:
                adjustBrightness(32);
                break;
            case ITEM_FONT_SIZE:
                cycleFontSize();
                break;
            case ITEM_GAME_SPEED:
                cycleGameSpeed();
                break;
            case ITEM_IDLE_TIME:
                cycleIdleTime();
                break;
            case ITEM_RESET:
                confirmReset = true;
                break;
            case ITEM_BACK:
                nextScene = SCENE_MENU;
                break;
        }
        return true;
    }

    return false;
}

SceneID SettingsScene::update() {
    return nextScene;
}

void SettingsScene::render() {
    PixelRenderer::fillRect(0, 0, 240, 135, PixelRenderer::BLACK);
    PixelRenderer::fillRect(0, 0, 240, 24, PixelRenderer::rgb565(25, 25, 40));
    PixelRenderer::drawPixelText(4, 4, "SETTINGS", PixelRenderer::CYAN);

    if (confirmReset) {
        renderConfirmReset();
    } else {
        renderMenu();
    }
}

void SettingsScene::renderConfirmReset() {
    PixelRenderer::fillRect(20, 40, 200, 60, PixelRenderer::rgb565(40, 40, 50));
    PixelRenderer::drawPixelText(35, 55, "Reset save?", PixelRenderer::YELLOW, 1);
    PixelRenderer::drawPixelText(35, 80, "A:Yes  B/Long:No", PixelRenderer::WHITE, 1);
}

void SettingsScene::renderMenu() {
    const char* items[ITEM_COUNT] = {
        "Brightness",
        "Font Size",
        "Game Speed",
        "Idle Time",
        "Reset Save",
        "Back",
    };

    float fs = PixelRenderer::getContentFontScale();
    int rowStep = (int)(26 * fs);
    int valOffset = (int)(12 * fs);
    int sepGap  = (int)(4 * fs);

    for (int i = 0; i < ITEM_COUNT; i++) {
        int y = 28 + i * rowStep;
        uint16_t color = (i == cursor) ? PixelRenderer::YELLOW : PixelRenderer::WHITE;

        if (i == cursor) {
            PixelRenderer::fillRect(4, y + (int)(4 * fs), 4, (int)(8 * fs), PixelRenderer::YELLOW);
        }

        PixelRenderer::drawPixelText(14, y, items[i], color);

        int valY = y + valOffset;
        char buf[16];
        if (i == ITEM_BRIGHTNESS) {
            snprintf(buf, sizeof(buf), "%d", Hal::ins().getBrightness());
            PixelRenderer::drawPixelText(14, valY, buf, PixelRenderer::CYAN, 1);
        } else if (i == ITEM_FONT_SIZE) {
            float curFs = PixelRenderer::getContentFontScale();
            if (curFs < 1.1f)       snprintf(buf, sizeof(buf), "1");
            else if (curFs < 1.3f)  snprintf(buf, sizeof(buf), "2");
            else if (curFs < 1.6f)  snprintf(buf, sizeof(buf), "3");
            else                    snprintf(buf, sizeof(buf), "4");
            PixelRenderer::drawPixelText(14, valY, buf, PixelRenderer::CYAN, 1);
        } else if (i == ITEM_GAME_SPEED) {
            snprintf(buf, sizeof(buf), "%.1fx", GameEngine::ins().getGameSpeed());
            PixelRenderer::drawPixelText(14, valY, buf, PixelRenderer::CYAN, 1);
        } else if (i == ITEM_IDLE_TIME) {
            uint8_t idx = GameEngine::ins().getIdleTimeoutIndex();
            const char* labels[5] = { "30s", "1m", "2m", "5m", "Never" };
            PixelRenderer::drawPixelText(14, valY, labels[idx], PixelRenderer::CYAN, 1);
        }

        if (i < ITEM_COUNT - 1) {
            PixelRenderer::fillRect(4, y + rowStep - sepGap, 232, 1, PixelRenderer::GRAY);
        }
    }
}

void SettingsScene::cycleFontSize() {
    float fs = PixelRenderer::getContentFontScale();
    if (fs < 1.1f)       GameEngine::ins().setFontScale(1.25f);
    else if (fs < 1.3f)  GameEngine::ins().setFontScale(1.5f);
    else if (fs < 1.6f)  GameEngine::ins().setFontScale(1.75f);
    else                 GameEngine::ins().setFontScale(1.0f);
    saveSettingsNow();
}

void SettingsScene::adjustBrightness(int delta) {
    uint8_t bri = Hal::ins().getBrightness();
    bri += delta;
    if (bri > 255 || bri < 32) bri = 32;
    Hal::ins().setBrightness(bri);
    saveSettingsNow();
}

void SettingsScene::cycleGameSpeed() {
    float sp = GameEngine::ins().getGameSpeed();
    if (sp < 0.6f)       GameEngine::ins().setGameSpeed(1.0f);
    else if (sp < 1.1f)  GameEngine::ins().setGameSpeed(2.0f);
    else if (sp < 2.1f)  GameEngine::ins().setGameSpeed(4.0f);
    else if (sp < 4.1f)  GameEngine::ins().setGameSpeed(8.0f);
    else                 GameEngine::ins().setGameSpeed(0.5f);
    saveSettingsNow();
}

void SettingsScene::cycleIdleTime() {
    uint8_t idx = GameEngine::ins().getIdleTimeoutIndex();
    idx++;
    if (idx > 4) idx = 0;
    GameEngine::ins().setIdleTimeoutIndex(idx);
    saveSettingsNow();
}

void SettingsScene::saveSettingsNow() {
    SaveManager::ins().saveSettings(
        PixelRenderer::getContentFontScale(),
        Hal::ins().getBrightness(),
        GameEngine::ins().getGameSpeed(),
        GameEngine::ins().getIdleTimeoutIndex()
    );
}
