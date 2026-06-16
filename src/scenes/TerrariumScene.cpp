#include "TerrariumScene.h"
#include "../core/GameEngine.h"
#include "../hardware/Hal.h"

const uint16_t TerrariumScene::PALETTE[4][2] = {
    { PixelRenderer::BROWN, PixelRenderer::DARK_BROWN },
    { 0x0400, 0x0200 },   // 深绿/暗绿
    { 0xFE00, 0xA600 },   // 金色/暗金
    { 0xE71C, 0x8010 },   // 白化淡紫/暗紫
};

void TerrariumScene::onEnter() {
    animFrame = 0;
    resetPressStart = 0;
    resetting = false;
}

void TerrariumScene::onExit() {}

SceneID TerrariumScene::update() {
    animFrame++;

    // 死亡后检测长按 A+B 3 秒重置
    Bug& bug = GameEngine::ins().getBug();
    if (bug.isDead()) {
        bool a = Hal::ins().btnA_raw();
        bool b = Hal::ins().btnB_raw();
        if (a && b) {
            if (resetPressStart == 0) resetPressStart = Hal::ins().millis();
            else if (Hal::ins().millis() - resetPressStart >= 3000) {
                bug.resetAfterDeath(GameEngine::ins().getGameNow());
                resetPressStart = 0;
            }
        } else {
            resetPressStart = 0;
        }
        return SCENE_NONE;
    }

    // 成虫简单游走动画
    if (bug.getStage() == Stage::ADULT) {
        int dx = (animFrame % 120 < 60) ? 1 : -1;
        bugX += dx;
        if (bugX < 30) bugX = 30;
        if (bugX > 170) bugX = 170;
    } else {
        bugX = 120;
    }

    return nextScene;
}

void TerrariumScene::render() {
    Bug& bug = GameEngine::ins().getBug();

    drawBackground();
    drawFoodTray();
    drawWood();
    drawBug();
    drawStatusBar();

    if (bug.isDead()) {
        drawDeathScreen();
    }
}

bool TerrariumScene::onButton(const ButtonEvent& ev) {
    Bug& bug = GameEngine::ins().getBug();
    if (bug.isDead()) return false;  // 死亡画面只接受 A+B 长按，由 update 处理

    if (ev.btn == 0 && ev.action == BtnAction::PRESSED) {
        // 短按 A：喂食
        if (bug.placeSapInTray()) {
            Serial.println("[Terrarium] Fed bug");
        } else {
            Serial.println("[Terrarium] Feed failed");
        }
        return true;
    }
    if (ev.btn == 1 && ev.action == BtnAction::PRESSED) {
        // 短按 B：戳
        if (bug.poke(GameEngine::ins().getGameNow())) {
            Serial.println("[Terrarium] Poked bug");
        }
        return true;
    }
    if (ev.btn == 0 && ev.action == BtnAction::LONG_PRESS) {
        // 长按 A：菜单
        nextScene = SCENE_MENU;
        return true;
    }
    return false;
}

void TerrariumScene::drawBackground() {
    PixelRenderer::fillRect(0, 0, 240, 135, PixelRenderer::rgb565(30, 40, 30));  // 深绿黑底
    // 底材
    PixelRenderer::fillRect(0, 120, 200, 15, PixelRenderer::rgb565(60, 50, 35));
}

void TerrariumScene::drawBug() {
    Bug& bug = GameEngine::ins().getBug();
    uint8_t pal = bug.getPaletteId();

    switch (bug.getStage()) {
        case Stage::EGG:
            drawEgg(bugX, 95, pal);
            break;
        case Stage::LARVA:
            drawLarva(bugX, 100, pal);
            break;
        case Stage::PUPA:
            drawPupa(bugX, 95, pal);
            break;
        case Stage::ADULT:
        default:
            drawAdult(bugX, 95, pal);
            break;
    }
}

void TerrariumScene::drawEgg(int x, int y, uint8_t palette) {
    (void)palette;
    uint16_t c = PixelRenderer::WHITE;
    PixelRenderer::fillRect(x - 6, y - 7, 12, 14, c);
    PixelRenderer::fillRect(x - 4, y - 9, 8, 2, c);
    PixelRenderer::fillRect(x - 4, y + 7, 8, 2, c);
}

void TerrariumScene::drawLarva(int x, int y, uint8_t palette) {
    uint16_t body = PixelRenderer::CREAM;
    uint16_t outline = PALETTE[palette][1];
    // 三段身体
    PixelRenderer::fillRect(x - 12, y - 4, 8, 8, body);
    PixelRenderer::fillRect(x - 4, y - 5, 8, 10, body);
    PixelRenderer::fillRect(x + 4, y - 4, 8, 8, body);
    PixelRenderer::fillRect(x - 14, y - 2, 2, 4, outline);
    PixelRenderer::fillRect(x + 12, y - 2, 2, 4, outline);
}

void TerrariumScene::drawPupa(int x, int y, uint8_t palette) {
    uint16_t body = PALETTE[palette][1];
    PixelRenderer::fillRect(x - 7, y - 9, 14, 18, body);
    PixelRenderer::fillRect(x - 5, y - 11, 10, 2, body);
}

void TerrariumScene::drawAdult(int x, int y, uint8_t palette) {
    uint16_t body = PALETTE[palette][0];
    uint16_t dark = PALETTE[palette][1];
    // 头胸部
    PixelRenderer::fillRect(x - 8, y - 6, 16, 12, body);
    // 腹部
    PixelRenderer::fillRect(x + 8, y - 8, 12, 16, body);
    // 角
    PixelRenderer::fillRect(x - 10, y - 10, 4, 6, dark);
    PixelRenderer::fillRect(x - 14, y - 14, 4, 6, dark);
    // 腿
    PixelRenderer::fillRect(x - 6, y + 6, 2, 6, dark);
    PixelRenderer::fillRect(x + 2, y + 6, 2, 6, dark);
    PixelRenderer::fillRect(x + 10, y + 8, 2, 5, dark);
}

void TerrariumScene::drawFoodTray() {
    // 食物盘在左下
    PixelRenderer::fillRect(20, 105, 30, 8, PixelRenderer::GRAY);
    Bug& bug = GameEngine::ins().getBug();
    if (bug.hasFoodInTray()) {
        PixelRenderer::fillRect(30, 100, 10, 5, PixelRenderer::YELLOW);
    }
}

void TerrariumScene::drawWood() {
    Bug& bug = GameEngine::ins().getBug();
    if (!bug.isWoodPlaced()) return;
    // 腐木在左上
    PixelRenderer::fillRect(30, 30, 40, 14, PixelRenderer::DARK_BROWN);
    PixelRenderer::fillRect(35, 32, 6, 2, PixelRenderer::BROWN);
    PixelRenderer::fillRect(55, 36, 8, 2, PixelRenderer::BROWN);
}

void TerrariumScene::drawStatusBar() {
    Bug& bug = GameEngine::ins().getBug();

    // 右侧状态栏背景
    PixelRenderer::fillRect(200, 0, 40, 135, PixelRenderer::BLACK);

    // 阶段图标
    const char* stageName = "?";
    switch (bug.getStage()) {
        case Stage::EGG: stageName = "E"; break;
        case Stage::LARVA: stageName = "L"; break;
        case Stage::PUPA: stageName = "P"; break;
        case Stage::ADULT: stageName = "A"; break;
    }
    PixelRenderer::drawPixelText(208, 4, stageName, PixelRenderer::WHITE, 2);

    // 饥饿条
    PixelRenderer::drawPixelText(204, 30, "HUN", PixelRenderer::WHITE, 1);
    uint16_t hungerColor = bug.getHunger() > 50 ? PixelRenderer::GREEN :
                           (bug.getHunger() > 20 ? PixelRenderer::YELLOW : PixelRenderer::RED);
    PixelRenderer::drawProgressBar(204, 42, 32, 6, bug.getHunger() / 100.0f, hungerColor, PixelRenderer::GRAY);

    // 背包
    char buf[16];
    snprintf(buf, sizeof(buf), "S:%d", bug.getSap());
    PixelRenderer::drawPixelText(204, 56, buf, PixelRenderer::WHITE, 1);
    snprintf(buf, sizeof(buf), "W:%d", bug.getRottenWood());
    PixelRenderer::drawPixelText(204, 68, buf, PixelRenderer::WHITE, 1);
}

void TerrariumScene::drawDeathScreen() {
    PixelRenderer::fillRect(40, 40, 160, 55, PixelRenderer::BLACK);
    PixelRenderer::fillRect(45, 45, 150, 45, PixelRenderer::RED);
    PixelRenderer::drawPixelText(55, 55, "DIED", PixelRenderer::WHITE, 3);
    PixelRenderer::drawPixelText(55, 80, "Hold A+B 3s", PixelRenderer::WHITE, 1);
}
