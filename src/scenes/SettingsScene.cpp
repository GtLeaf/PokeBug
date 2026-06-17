#include "SettingsScene.h"
#include "../core/GameEngine.h"
#include "../core/SaveManager.h"

void SettingsScene::onEnter() {
    cursor = 0;
    confirmReset = false;
    scrollY = 0;
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
            GameEngine::ins().setMainSceneBg(GameEngine::BG_MOSS);
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

    if (ev.action == BtnAction::LONG_PRESS) {
        if (ev.btn == 0) {
            // 长按 A：返回主界面（培养缸）
            nextScene = SCENE_TERRARIUM;
            return true;
        }
        if (ev.btn == 1) {
            // 长按 B：返回上级菜单
            nextScene = SCENE_MENU;
            return true;
        }
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
    PixelRenderer::fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::BLACK);

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
    int rowStep = (int)(14 * fs);
    int startY = 8;
    int sepGap  = (int)(4 * fs);
    LGFX_Sprite& canvas = Hal::ins().canvas();

    // 根据选中项调整纵向滚动偏移
    int contentH = startY + ITEM_COUNT * rowStep;
    if (contentH > Hal::DISPLAY_H) {
        int selTop = startY + cursor * rowStep;
        int selBottom = selTop + rowStep;
        if (selBottom - scrollY > Hal::DISPLAY_H) {
            scrollY = selBottom - Hal::DISPLAY_H;
        }
        if (selTop - scrollY < 0) {
            scrollY = selTop;
        }
    } else {
        scrollY = 0;
    }

    for (int i = 0; i < ITEM_COUNT; i++) {
        int y = startY + i * rowStep - scrollY;
        uint16_t color = (i == cursor) ? PixelRenderer::YELLOW : PixelRenderer::WHITE;

        if (i == cursor) {
            PixelRenderer::fillRect(4, y, 4, (int)(8 * fs), PixelRenderer::YELLOW);
        }

        PixelRenderer::drawPixelText(14, y, items[i], color);

        char buf[16];
        bool hasValue = false;
        if (i == ITEM_BRIGHTNESS) {
            snprintf(buf, sizeof(buf), "%d", Hal::ins().getBrightness());
            hasValue = true;
        } else if (i == ITEM_FONT_SIZE) {
            float curFs = PixelRenderer::getContentFontScale();
            if (curFs < 1.1f)       snprintf(buf, sizeof(buf), "1");
            else if (curFs < 1.3f)  snprintf(buf, sizeof(buf), "2");
            else if (curFs < 1.6f)  snprintf(buf, sizeof(buf), "3");
            else                    snprintf(buf, sizeof(buf), "4");
            hasValue = true;
        } else if (i == ITEM_GAME_SPEED) {
            snprintf(buf, sizeof(buf), "%.1fx", GameEngine::ins().getGameSpeed());
            hasValue = true;
        } else if (i == ITEM_IDLE_TIME) {
            uint8_t idx = GameEngine::ins().getIdleTimeoutIndex();
            const char* labels[5] = { "30s", "1m", "2m", "5m", "Never" };
            snprintf(buf, sizeof(buf), "%s", labels[idx]);
            hasValue = true;
        }

        if (hasValue) {
            canvas.setTextSize(fs);
            int valW = canvas.textWidth(buf);
            PixelRenderer::drawPixelText(Hal::DISPLAY_W - valW - 8, y, buf, PixelRenderer::CYAN, fs);
        }

        if (i < ITEM_COUNT - 1) {
            PixelRenderer::fillRect(4, y + rowStep - sepGap, Hal::DISPLAY_W - 8, 1, PixelRenderer::GRAY);
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
        GameEngine::ins().getIdleTimeoutIndex(),
        GameEngine::ins().getMainSceneBg()
    );
}
