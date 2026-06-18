#include "TerrariumScene.h"
#include "../core/GameEngine.h"
#include "../hardware/Hal.h"
#include "../assets/MainSceneAssets.h"
#include "../assets/HerculesAdultSprites.h"
#include "../assets/WoodAssets.h"

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

    // 初始化成虫状态
    adultState = AdultState::IDLE;
    faceRight = true;
    turnTargetFaceRight = true;
    walkAfterTurn = false;
    turnFrameIndex = 0;
    targetX = bugX;
    stateTimer = 0;
    stateDuration = random(30, 90);
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

    // 成虫使用状态机自然移动：贴地、走走停停、有食物会靠近吃
    if (bug.getStage() == Stage::ADULT && !bug.isDead()) {
        updateAdultMovement();
    } else if (bug.getStage() != Stage::ADULT) {
        bugX = 120;
        faceRight = true;
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
    if (GameEngine::ins().getMainSceneBg() == GameEngine::BG_BEGINNER) {
        PixelRenderer::drawRgb565(0, 0,
                                  MainSceneAssets::BEGINNER_FULL_W,
                                  MainSceneAssets::BEGINNER_FULL_H,
                                  MainSceneAssets::BEGINNER_FULL);
        return;
    }
    if (GameEngine::ins().getMainSceneBg() == GameEngine::BG_CHILD_ROOM) {
        PixelRenderer::drawRgb565(0, 0,
                                  MainSceneAssets::CHILD_ROOM_FULL_W,
                                  MainSceneAssets::CHILD_ROOM_FULL_H,
                                  MainSceneAssets::CHILD_ROOM_FULL);
        return;
    }

    PixelRenderer::drawRgb565(0, 0,
                              MainSceneAssets::MOSS_BG_W,
                              MainSceneAssets::MOSS_BG_H,
                              MainSceneAssets::MOSS_BG);
    PixelRenderer::drawRgb565(0, 120,
                              MainSceneAssets::MOSS_GROUND_W,
                              MainSceneAssets::MOSS_GROUND_H,
                              MainSceneAssets::MOSS_GROUND);
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
            drawAdult(bugX, GROUND_Y, pal);
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
    (void)palette;

    Bug& bug = GameEngine::ins().getBug();
    const HerculesAdultSprites::RleFrame* frames = HerculesAdultSprites::WALK_FRAMES;
    const uint16_t* data = HerculesAdultSprites::WALK_RLE;
    uint8_t frameCount = HerculesAdultSprites::WALK_FRAME_COUNT;
    uint8_t frameIndex = (animFrame / 10) % frameCount;
    bool flipSprite = !faceRight;

    if (bug.isDead()) {
        // 死亡叠层：使用 reset 第 0 帧
        frames = HerculesAdultSprites::RESET_FRAMES;
        data = HerculesAdultSprites::RESET_RLE;
        frameCount = HerculesAdultSprites::RESET_FRAME_COUNT;
        frameIndex = 0;
        flipSprite = !faceRight;
    } else if (adultState == AdultState::EAT) {
        // 进食动画
        frames = HerculesAdultSprites::EAT_FRAMES;
        data = HerculesAdultSprites::EAT_RLE;
        frameCount = HerculesAdultSprites::EAT_FRAME_COUNT;
        frameIndex = (animFrame / 16) % frameCount;
        flipSprite = !faceRight;
    } else if (adultState == AdultState::TURN) {
        // 转身过渡每次随机取一张中间姿态；同一次转身中保持不变。
        frames = HerculesAdultSprites::TURN_FRAMES;
        data = HerculesAdultSprites::TURN_RLE;
        frameCount = HerculesAdultSprites::TURN_FRAME_COUNT;
        frameIndex = turnFrameIndex;
        if (frameIndex >= frameCount) frameIndex = frameCount - 1;
        flipSprite = false;
    } else if (adultState == AdultState::IDLE) {
        // 静止时停在 walk 第 0 帧（站立姿态）
        frameIndex = 0;
        flipSprite = !faceRight;
    }

    uint16_t offset = pgm_read_word(&frames[frameIndex].offset);
    uint16_t length = pgm_read_word(&frames[frameIndex].length);
    // y 是脚/底部参考点，让精灵底部对齐 y
    PixelRenderer::drawRgb565Rle(x - HerculesAdultSprites::FRAME_W / 2,
                                 y - HerculesAdultSprites::FRAME_H,
                                 HerculesAdultSprites::FRAME_W,
                                 HerculesAdultSprites::FRAME_H,
                                 data, offset, length, flipSprite);
}

// 判断成虫是否想去进食：饥饿或纯粹嘴馋
bool TerrariumScene::wantsToEat() {
    Bug& bug = GameEngine::ins().getBug();
    if (!bug.hasFoodInTray() || bug.getFoodAmount() == 0) return false;
    if (bug.getHunger() < 60) return true;
    // 饱腹时也有小概率去闻闻食物
    return random(100) < 10;
}

void TerrariumScene::startTurn(bool targetFaceRight, bool continueWalking) {
    adultState = AdultState::TURN;
    turnTargetFaceRight = targetFaceRight;
    walkAfterTurn = continueWalking;
    turnFrameIndex = random(HerculesAdultSprites::TURN_FRAME_COUNT);
    stateTimer = 0;
    stateDuration = TURN_DURATION_FRAMES;
}

// 开始向目标位置行走
void TerrariumScene::startWalkTo(int x) {
    targetX = x;
    if (targetX < MIN_X) targetX = MIN_X;
    if (targetX > MAX_X) targetX = MAX_X;

    bool needFaceRight = (targetX > bugX);
    if (needFaceRight != faceRight) {
        startTurn(needFaceRight, true);
    } else {
        adultState = AdultState::WALK;
        stateTimer = 0;
        stateDuration = 0;   // 走到目的地为止
    }
}

// 成虫状态机：贴地、走走停停、靠近食物进食
void TerrariumScene::updateAdultMovement() {
    Bug& bug = GameEngine::ins().getBug();
    stateTimer++;

    switch (adultState) {
        case AdultState::IDLE:
            // 静止时偶尔张望（朝向随机小概率翻转）
            if (stateTimer > stateDuration / 2 && random(100) < 3) {
                startTurn(!faceRight, false);
                break;
            }
            // 静止结束后决定下一步
            if (stateTimer >= stateDuration) {
                if (wantsToEat()) {
                    startWalkTo(FOOD_X);
                } else {
                    // 随机巡逻点，避免连续两次太近
                    int newTarget = random(MIN_X, MAX_X + 1);
                    if (abs(newTarget - bugX) < 30) {
                        newTarget = (bugX < 120) ? random(140, MAX_X + 1)
                                                   : random(MIN_X, 100);
                    }
                    startWalkTo(newTarget);
                }
            }
            break;

        case AdultState::TURN:
            // 转身动画完成后朝向已改变，进入行走
            if (stateTimer >= stateDuration) {
                faceRight = turnTargetFaceRight;
                stateTimer = 0;
                if (walkAfterTurn) {
                    adultState = AdultState::WALK;
                    stateDuration = 0;
                } else {
                    adultState = AdultState::IDLE;
                    stateDuration = random(30, 90);
                }
            }
            break;

        case AdultState::WALK:
            {
                int dx = (targetX > bugX) ? 1 : -1;
                faceRight = (dx > 0);

                // 每 3 帧移动 1 像素，模拟甲虫缓慢爬行；进食心切时走快点
                uint8_t stepInterval = wantsToEat() ? 2 : 3;
                if (stateTimer % stepInterval == 0) {
                    bugX += dx;
                    if (bugX < MIN_X) bugX = MIN_X;
                    if (bugX > MAX_X) bugX = MAX_X;
                }

                // 到达目标
                if (abs(targetX - bugX) <= 2) {
                    if (wantsToEat() && targetX == FOOD_X) {
                        adultState = AdultState::EAT;
                        stateTimer = 0;
                        stateDuration = random(180, 420);  // 吃 3-7 秒
                    } else {
                        adultState = AdultState::IDLE;
                        stateTimer = 0;
                        stateDuration = random(40, 150);   // 停 0.7-2.5 秒
                    }
                }
            }
            break;

        case AdultState::EAT:
            // 面向食物盘（食物在左侧，所以朝左）
            faceRight = false;
            // 吃到食物消失、吃饱或超时后离开
            if (!bug.hasFoodInTray() || bug.getFoodAmount() == 0 ||
                bug.getHunger() >= 95 || stateTimer >= stateDuration) {
                adultState = AdultState::IDLE;
                stateTimer = 0;
                stateDuration = random(30, 90);
            }
            break;
    }
}

void TerrariumScene::drawFoodTray() {
    static constexpr int TRAY_X = 20;
    static constexpr int TRAY_W = 30;
    static constexpr int TRAY_H = 8;
    static constexpr int TRAY_Y = GROUND_Y - TRAY_H;

    PixelRenderer::fillRect(TRAY_X, TRAY_Y, TRAY_W, TRAY_H, PixelRenderer::GRAY);
    Bug& bug = GameEngine::ins().getBug();
    if (bug.hasFoodInTray() && bug.getFoodAmount() > 0) {
        // 食物宽度随剩余量变化，最小保留 2 像素
        uint8_t w = (bug.getFoodAmount() * 10) / Bug::FOOD_MAX_AMOUNT;
        if (w < 2) w = 2;
        uint8_t x = TRAY_X + (TRAY_W - w) / 2;
        PixelRenderer::fillRect(x, TRAY_Y - 5, w, 5, PixelRenderer::YELLOW);
    }
}

void TerrariumScene::drawWood() {
    Bug& bug = GameEngine::ins().getBug();
    if (!bug.isWoodPlaced()) return;

    uint8_t style = GameEngine::ins().getWoodStyle();
    if (style >= WoodAssets::WOOD_COUNT) style = 0;
    uint16_t offset = pgm_read_word(&WoodAssets::WOOD_FRAMES[style].offset);
    uint16_t length = pgm_read_word(&WoodAssets::WOOD_FRAMES[style].length);
    PixelRenderer::drawRgb565Rle(200 - WoodAssets::FRAME_W,
                                 GROUND_Y - WoodAssets::FRAME_H,
                                 WoodAssets::FRAME_W,
                                 WoodAssets::FRAME_H,
                                 WoodAssets::WOOD_RLE,
                                 offset, length);
}

void TerrariumScene::drawStatusBar() {
    Bug& bug = GameEngine::ins().getBug();

    if (GameEngine::ins().getMainSceneBg() == GameEngine::BG_MOSS) {
        PixelRenderer::drawRgb565(200, 0,
                                  MainSceneAssets::MOSS_STATE_W,
                                  MainSceneAssets::MOSS_STATE_H,
                                  MainSceneAssets::MOSS_STATE);
    }

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
