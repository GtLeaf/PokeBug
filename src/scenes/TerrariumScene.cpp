#include "TerrariumScene.h"
#include "../core/GameEngine.h"
#include "../hardware/Hal.h"
#include "../assets/MainSceneAssets.h"
#include "../assets/HerculesAdultSprites.h"
#include "../assets/WoodAssets.h"
#include "../assets/BowlAssets.h"
#include "../assets/FoodAssets.h"
#include "../assets/ActionAssets.h"
#include <cmath>

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
    slideAfterTurn = false;
    climbAfterTurn = false;
    turnFrameIndex = 0;
    targetX = bugX;
    slideTargetX = bugX;
    climbTargetX = bugX;
    tiltHighSideIsRight = true;
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

    // 成虫倾斜引导：读取 IMU 并更新方向
    if (bug.getStage() == Stage::ADULT && !bug.isDead()) {
        updateTilt();
    }

    // 戳反应计时
    if (pokeReactionEndMs != 0 && Hal::ins().millis() > pokeReactionEndMs) {
        pokeReactionEndMs = 0;
    }
    if (pokeThreatenEndMs != 0 && Hal::ins().millis() > pokeThreatenEndMs) {
        pokeThreatenEndMs = 0;
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
    drawPokeAction();
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
        uint64_t gameNow = GameEngine::ins().getGameNow();
        bool ok = bug.poke(gameNow);
        if (ok) {
            uint32_t now = Hal::ins().millis();
            // 若已在 threaten 持续阶段（已播完动作、正 hold 最后一帧），只重置持续时间，
            // 不要从第 0 帧重新播放。
            bool inThreatenHold = (pokeThreatenEndMs != 0) &&
                                  (now >= pokeReactionStartMs + THREATEN_PLAY_MS);
            if (!inThreatenHold) {
                pokeReactionStartMs = now;
            }
            pokeReactionEndMs = now + 600;  // 手指动画保持 600ms
            pokeThreatenEndMs = now + POKE_REACTION_MS;  // 威吓保持约 3s
            pokeReactionWasPoked = true;
            pokeFingerFromRight = random(2) == 0;
            pokeFingerFrameIndex = random(ActionAssets::FINGER_FRAME_COUNT);
            pokeFingerYOffset = (int8_t)random(-3, 4);
            if (bug.getStage() == Stage::ADULT) {
                // 成虫朝向戳来的方向后退戒备
                faceRight = pokeFingerFromRight;
                turnTargetFaceRight = faceRight;
                int dir = pokeFingerFromRight ? -1 : 1;
                bugX += dir * 5;
                if (bugX < MIN_X) bugX = MIN_X;
                if (bugX > MAX_X) bugX = MAX_X;
                adultState = AdultState::IDLE;
                stateTimer = 0;
                stateDuration = random(30, 90);
            }
            Serial.println("[Terrarium] Poked bug");
        } else {
            // 冷却中：短提示
            pokeReactionStartMs = Hal::ins().millis();
            pokeReactionEndMs = Hal::ins().millis() + 300;
            pokeReactionWasPoked = false;
            Serial.println("[Terrarium] Poke on cooldown");
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

    // 戳反应覆盖绘制
    if (pokeReactionEndMs != 0 && Hal::ins().millis() < pokeReactionEndMs) {
        if (pokeReactionWasPoked) {
            if (bug.getStage() == Stage::EGG) {
                int shake = ((Hal::ins().millis() / 50) % 2) ? 2 : -2;
                drawEgg(bugX + shake, 95, pal);
                return;
            }
            if (bug.getStage() == Stage::LARVA) {
                drawLarvaPoked(bugX, 100, pal);
                return;
            }
            if (bug.getStage() == Stage::PUPA) {
                drawPupa(bugX, 95, pal);
                return;
            }
            // 成虫：fall through 到 drawAdult，其内部会处理 poke 反应（威吓姿态）
        } else {
            // 冷却中：在甲虫上方画闪烁提示
            int baseY = 95;
            if (bug.getStage() == Stage::LARVA) baseY = 100;
            else if (bug.getStage() == Stage::ADULT) baseY = GROUND_Y;
            drawPokeCooldownHint(bugX, baseY);
        }
    }

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
    } else if (pokeThreatenEndMs != 0 && pokeReactionWasPoked && bug.getStage() == Stage::ADULT) {
        // 戳反应：威吓动画，前 THREATEN_PLAY_MS 播放，之后保持最后一帧
        frames = HerculesAdultSprites::THREATEN_FRAMES;
        data = HerculesAdultSprites::THREATEN_RLE;
        frameCount = HerculesAdultSprites::THREATEN_FRAME_COUNT;
        uint32_t elapsed = Hal::ins().millis() - pokeReactionStartMs;
        if (elapsed < THREATEN_PLAY_MS) {
            frameIndex = (elapsed * frameCount) / THREATEN_PLAY_MS;
            if (frameIndex >= frameCount) frameIndex = frameCount - 1;
        } else {
            frameIndex = frameCount - 1;
        }
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
    } else if (adultState == AdultState::SLIDE) {
        // 滑落：保持当前姿势滑下去，不播放 walk 动画
        frameIndex = 0;
        flipSprite = !faceRight;
    } else if (adultState == AdultState::CLIMB) {
        // 爬坡：使用 walk 帧，很慢播放
        frameIndex = (animFrame / 20) % frameCount;
        flipSprite = !faceRight;
    } else if (adultState == AdultState::IDLE) {
        // 静止时停在 walk 第 0 帧（站立姿态）
        frameIndex = 0;
        flipSprite = !faceRight;
    }

    uint16_t offset = pgm_read_word(&frames[frameIndex].offset);
    uint16_t length = pgm_read_word(&frames[frameIndex].length);
    uint8_t frameW = pgm_read_byte(&frames[frameIndex].width);
    uint8_t frameH = pgm_read_byte(&frames[frameIndex].height);
    // y 是脚/底部参考点，让精灵底部对齐 y
    PixelRenderer::drawRgb565Rle(x - frameW / 2,
                                 y - frameH,
                                 frameW,
                                 frameH,
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

    // threaten（威吓）期间不主动移动，但允许因倾斜而滑落
    if (pokeThreatenEndMs != 0 && Hal::ins().millis() < pokeThreatenEndMs) {
        if (adultState != AdultState::SLIDE) {
            return;
        }
    }

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
            // 转身动画完成后朝向已改变，进入对应后续状态
            if (stateTimer >= stateDuration) {
                faceRight = turnTargetFaceRight;
                stateTimer = 0;
                if (slideAfterTurn) {
                    slideAfterTurn = false;
                    adultState = AdultState::SLIDE;
                    stateDuration = 0;
                } else if (climbAfterTurn) {
                    climbAfterTurn = false;
                    adultState = AdultState::CLIMB;
                    stateDuration = 0;
                } else if (walkAfterTurn) {
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
                        stateDuration = random(40, 150);
                    }
                }
            }
            break;

        case AdultState::SLIDE:
            // 大角度倾斜：快速向低处滑落
            {
                int dx = (slideTargetX > bugX) ? 1 : -1;
                faceRight = (dx > 0);
                if (stateTimer % 1 == 0) {
                    bugX += dx * TILT_SLIDE_STEP;
                    if (bugX < MIN_X) bugX = MIN_X;
                    if (bugX > MAX_X) bugX = MAX_X;
                }
                if (bugX == slideTargetX ||
                    (dx > 0 && bugX >= slideTargetX) ||
                    (dx < 0 && bugX <= slideTargetX)) {
                    Serial.printf("[Tilt] slide done, turn to climb -> target=%d\n",
                                  climbTargetX);
                    bool needFaceRight = tiltHighSideIsRight;
                    if (needFaceRight != faceRight) {
                        startTurn(needFaceRight, true);
                        climbAfterTurn = true;
                    } else {
                        startClimbOrIdle();
                    }
                }
            }
            break;

        case AdultState::CLIMB:
            // 向高处缓慢爬行
            {
                int dx = (climbTargetX > bugX) ? 1 : -1;
                faceRight = (dx > 0);
                if (stateTimer % TILT_CLIMB_SPEED_INTERVAL == 0) {
                    bugX += dx;
                    if (bugX < MIN_X) bugX = MIN_X;
                    if (bugX > MAX_X) bugX = MAX_X;
                }
                if (abs(climbTargetX - bugX) <= 1) {
                    Serial.println("[Tilt] climb done -> IDLE");
                    adultState = AdultState::IDLE;
                    stateTimer = 0;
                    stateDuration = random(30, 90);
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
    static constexpr int BOWL_X = 14;
    static constexpr int BOWL_Y = GROUND_Y - BowlAssets::FRAME_H;

    uint8_t style = GameEngine::ins().getBowlStyle();
    if (style >= BowlAssets::SPRITE_COUNT) style = 0;
    uint16_t bowlOffset = pgm_read_word(&BowlAssets::SPRITE_FRAMES[style].offset);
    uint16_t bowlLength = pgm_read_word(&BowlAssets::SPRITE_FRAMES[style].length);
    PixelRenderer::drawRgb565Rle(BOWL_X, BOWL_Y,
                                 BowlAssets::FRAME_W,
                                 BowlAssets::FRAME_H,
                                 BowlAssets::SPRITE_RLE,
                                 bowlOffset, bowlLength);

    Bug& bug = GameEngine::ins().getBug();
    if (bug.hasFoodInTray() && bug.getFoodAmount() > 0) {
        uint8_t foodStyle = GameEngine::ins().getFoodStyle();
        if (foodStyle >= FoodAssets::SPRITE_COUNT) foodStyle = 0;
        uint16_t foodOffset = pgm_read_word(&FoodAssets::SPRITE_FRAMES[foodStyle].offset);
        uint16_t foodLength = pgm_read_word(&FoodAssets::SPRITE_FRAMES[foodStyle].length);
        PixelRenderer::drawRgb565RleEaten(BOWL_X + (BowlAssets::FRAME_W - FoodAssets::FRAME_W) / 2,
                                          BOWL_Y + 3,
                                          FoodAssets::FRAME_W,
                                          FoodAssets::FRAME_H,
                                          FoodAssets::SPRITE_RLE,
                                          foodOffset, foodLength,
                                          bug.getFoodAmount(),
                                          Bug::FOOD_MAX_AMOUNT);
    }
}

void TerrariumScene::drawWood() {
    Bug& bug = GameEngine::ins().getBug();
    if (!bug.isWoodPlaced()) return;

    uint8_t style = GameEngine::ins().getWoodStyle();
    if (style >= WoodAssets::SPRITE_COUNT) style = 0;
    uint16_t offset = pgm_read_word(&WoodAssets::SPRITE_FRAMES[style].offset);
    uint16_t length = pgm_read_word(&WoodAssets::SPRITE_FRAMES[style].length);
    PixelRenderer::drawRgb565Rle(200 - WoodAssets::FRAME_W,
                                 GROUND_Y - WoodAssets::FRAME_H + 5,
                                 WoodAssets::FRAME_W,
                                 WoodAssets::FRAME_H,
                                 WoodAssets::SPRITE_RLE,
                                 offset, length);
}

void TerrariumScene::drawStatusBar() {
    Bug& bug = GameEngine::ins().getBug();
    LGFX_Sprite& canvas = Hal::ins().canvas();
    float fs = 1.0f;

    // 状态栏背景（Moss 主题）
    if (GameEngine::ins().getMainSceneBg() == GameEngine::BG_MOSS) {
        PixelRenderer::drawRgb565(200, 0,
                                  MainSceneAssets::MOSS_STATE_W,
                                  MainSceneAssets::MOSS_STATE_H,
                                  MainSceneAssets::MOSS_STATE);
    }

    uint64_t gameNow = GameEngine::ins().getGameNow();

    // ---- 模拟环境（基于虚拟时间，后续与天气系统联动） ----
    float dayPhase = (gameNow % (24ULL * 60 * 60 * 1000)) / (24.0f * 60 * 60 * 1000);
    uint32_t virtualHour = (uint32_t)(gameNow / (60ULL * 60 * 1000));
    int tempNoise = (int)((virtualHour * 7) % 5) - 2;   // -2 ~ +2
    int humNoise  = (int)((virtualHour * 13) % 7) - 3;  // -3 ~ +3

    int temp = 25 + (int)(3.0f * sin(dayPhase * 2.0f * 3.1415926535f)) + tempNoise;
    if (temp < 18) temp = 18; else if (temp > 32) temp = 32;

    int hum = 58 - (int)(10.0f * sin(dayPhase * 2.0f * 3.1415926535f)) + humNoise;
    if (hum < 30) hum = 30; else if (hum > 85) hum = 85;

    // 虚拟时间（与游戏速度同步）
    uint32_t totalMinutes = (uint32_t)(gameNow / (60ULL * 1000));
    uint32_t day = totalMinutes / (24 * 60) + 1;
    uint32_t hour = (totalMinutes % (24 * 60)) / 60;
    uint32_t minute = totalMinutes % 60;

    // 布局工具
    static constexpr int BAR_X = 200;
    static constexpr int BAR_W = 40;

    auto drawSep = [&](int y) {
        PixelRenderer::fillRect(BAR_X + 2, y, BAR_W - 4, 1, PixelRenderer::GRAY);
    };

    auto centerText = [&](const char* text, int y, uint16_t color) {
        canvas.setTextSize(fs);
        int tw = canvas.textWidth(text);
        int x = BAR_X + (BAR_W - tw) / 2;
        PixelRenderer::drawPixelText(x, y, text, color, fs);
    };

    // 布局：简约疏朗，每个区块之间留出空白，无分隔线
    // 135px 高度均匀分布：阶段(8) 空(14) 饥饿(22) 空(14) MOT(38) 空(14) 温度(54) 湿度(68) 空(14) 时间(84)

    // ---- 1. 阶段图标 ----
    const char* stageName = "?";
    switch (bug.getStage()) {
        case Stage::EGG:   stageName = "E"; break;
        case Stage::LARVA: stageName = "L"; break;
        case Stage::PUPA:  stageName = "P"; break;
        case Stage::ADULT: stageName = "A"; break;
    }
    centerText(stageName, 8, PixelRenderer::WHITE);

    // ---- 2. 饥饿度（无标签，颜色即信息） ----
    uint16_t hColor = bug.getHunger() > 50 ? PixelRenderer::GREEN :
                      (bug.getHunger() > 20 ? PixelRenderer::YELLOW : PixelRenderer::RED);
    PixelRenderer::drawProgressBar(204, 22, 32, 6, bug.getHunger() / 100.0f, hColor, PixelRenderer::GRAY);

    // ---- 3. MOT：像素爱心，从底部像水位一样填充 ----
    uint8_t mot = bug.getMot();
    // 7x6 爱心掩码（MSB 在左，1=像素）
    static const uint8_t HEART_MASK[6] = {
        0b0110110,  // .##.##.
        0b1111111,  // #######
        0b1111111,  // #######
        0b0111110,  // .#####.
        0b0011100,  // ..###..
        0b0001000,  // ...#...
    };
    static constexpr int HEART_W = 7;
    static constexpr int HEART_H = 6;
    int heartX = BAR_X + (BAR_W - HEART_W) / 2;  // 居中
    int heartY = 38;
    int totalRows = HEART_H;
    int fillRows = (mot * totalRows + 50) / 100;
    if (fillRows < 1 && mot > 0) fillRows = 1;
    if (fillRows > totalRows) fillRows = totalRows;

    for (int row = 0; row < totalRows; row++) {
        uint8_t mask = HEART_MASK[row];
        bool fill = (row >= totalRows - fillRows);  // 从底部向上填充
        uint16_t c = fill ? PixelRenderer::RED : PixelRenderer::GRAY;
        for (int col = 0; col < HEART_W; col++) {
            if (mask & (1 << (HEART_W - 1 - col))) {
                PixelRenderer::fillRect(heartX + col, heartY + row, 1, 1, c);
            }
        }
    }

    // ---- 4. 温湿度（拉开间距） ----
    char buf[8];
    PixelRenderer::fillRect(BAR_X + 4, 54, 4, 4, PixelRenderer::YELLOW);
    snprintf(buf, sizeof(buf), "%d\u00b0", temp);
    PixelRenderer::drawPixelText(BAR_X + 10, 52, buf, PixelRenderer::WHITE, 1);

    PixelRenderer::fillRect(BAR_X + 4, 68, 4, 4, PixelRenderer::BLUE);
    snprintf(buf, sizeof(buf), "%d%%", hum);
    PixelRenderer::drawPixelText(BAR_X + 10, 66, buf, PixelRenderer::WHITE, 1);

    // ---- 5. 虚拟时间 ----
    char timeBuf[8];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", hour, minute);
    centerText(timeBuf, 84, PixelRenderer::WHITE);
}

void TerrariumScene::drawDeathScreen() {
    PixelRenderer::fillRect(40, 40, 160, 55, PixelRenderer::BLACK);
    PixelRenderer::fillRect(45, 45, 150, 45, PixelRenderer::RED);
    PixelRenderer::drawPixelText(55, 55, "DIED", PixelRenderer::WHITE, 3);
    PixelRenderer::drawPixelText(55, 80, "Hold A+B 3s", PixelRenderer::WHITE, 1);
}

// 倾斜检测：每帧读取 IMU，防抖 200ms 后触发
void TerrariumScene::updateTilt() {
    float ax, ay, az;
    Hal::ins().getAccel(ax, ay, az);

    // 垂直方向过滤：竖屏/举起时忽略，避免甲虫乱跑
    if (fabsf(ay) > 1.5f) {
        static uint32_t lastLog = 0;
        if (Hal::ins().millis() - lastLog >= 1000) {
            Serial.printf("[Tilt] ignore, ay=%.2f (vertical)\n", ay);
            lastLog = Hal::ins().millis();
        }
        return;
    }

    // 摇晃优先：剧烈摇晃时跳过倾斜，避免冲突
    float mag = Hal::ins().getAccelMagnitude();
    if (mag > 2.0f) {
        static uint32_t lastShakeLog = 0;
        if (Hal::ins().millis() - lastShakeLog >= 500) {
            Serial.printf("[Tilt] ignore, mag=%.2f (shake)\n", mag);
            lastShakeLog = Hal::ins().millis();
        }
        return;
    }

    TiltDir newDir = TiltDir::NONE;
    if (ax > TILT_THRESHOLD_G) newDir = TiltDir::LEFT;
    else if (ax < -TILT_THRESHOLD_G) newDir = TiltDir::RIGHT;

    const char* highSideStr = (newDir == TiltDir::LEFT) ? "RIGHT" :
                              (newDir == TiltDir::RIGHT) ? "LEFT" : "NONE";
    const char* lowSideStr  = (newDir == TiltDir::LEFT) ? "LEFT" :
                              (newDir == TiltDir::RIGHT) ? "RIGHT" : "NONE";

    // 方向变化时打印：明确显示高低方向
    if (newDir != pendingTiltDir) {
        Serial.printf("[Tilt] ax=%.2f pending low=%s/%d -> low=%s/%d  (high=%s)\n",
                      ax,
                      (pendingTiltDir == TiltDir::LEFT) ? "LEFT" :
                      (pendingTiltDir == TiltDir::RIGHT) ? "RIGHT" : "NONE",
                      (int)pendingTiltDir,
                      lowSideStr, (int)newDir, highSideStr);
        pendingTiltDir = newDir;
        tiltStableMs = Hal::ins().millis();
    }

    if (newDir != activeTiltDir &&
        Hal::ins().millis() - tiltStableMs >= TILT_DEBOUNCE_MS) {
        activeTiltDir = newDir;
        Serial.printf("[Tilt] STABLE low=%s/%d high=%s (stable %dms)\n",
                      lowSideStr, (int)activeTiltDir, highSideStr, TILT_DEBOUNCE_MS);
        if (activeTiltDir != TiltDir::NONE) {
            onTilt(activeTiltDir, mag);
        } else {
            // 恢复水平：如果正在爬坡，停下来休息
            if (adultState == AdultState::CLIMB) {
                Serial.println("[Tilt] level, stop climb -> IDLE");
                adultState = AdultState::IDLE;
                stateTimer = 0;
                stateDuration = random(30, 90);
            }
        }
    }
}

// 倾斜触发：
// - 小角度：转身朝高处，然后缓慢爬行或原地不动
// - 大角度：先向低处滑落一小段，再转身朝高处爬行
// dir 表示低处方向（设备哪一侧向下）
void TerrariumScene::onTilt(TiltDir dir, float magnitude) {
    const char* lowSideStr  = (dir == TiltDir::LEFT) ? "LEFT" : "RIGHT";
    const char* highSideStr = (dir == TiltDir::LEFT) ? "RIGHT" : "LEFT";
    Serial.printf("[Tilt] onTilt low=%s high=%s mag=%.2f adultState=%d bugX=%d\n",
                  lowSideStr, highSideStr, magnitude, (int)adultState, bugX);

    if (adultState == AdultState::EAT) {
        Serial.println("[Tilt] ignored: eating");
        return;
    }

    // threaten（威吓）期间只响应大角度滑落，不响应小角度爬行
    if (pokeThreatenEndMs != 0 && Hal::ins().millis() < pokeThreatenEndMs) {
        if (magnitude <= TILT_SLIDE_THRESHOLD_G) {
            Serial.println("[Tilt] ignored: threatening");
            return;
        }
    }

    // 低处在左 => 高处在右；低处在右 => 高处在左
    bool lowIsLeft = (dir == TiltDir::LEFT);
    tiltHighSideIsRight = !lowIsLeft;

    // 高处目标位置
    int highTarget = bugX + (tiltHighSideIsRight ? TILT_CLIMB_DISTANCE : -TILT_CLIMB_DISTANCE);
    if (highTarget < MIN_X) highTarget = MIN_X;
    if (highTarget > MAX_X) highTarget = MAX_X;
    climbTargetX = highTarget;

    if (magnitude > TILT_SLIDE_THRESHOLD_G) {
        // 大角度：先向低处滑落
        int lowTarget = bugX + (lowIsLeft ? -TILT_SLIDE_DISTANCE : +TILT_SLIDE_DISTANCE);
        if (lowTarget < MIN_X) lowTarget = MIN_X;
        if (lowTarget > MAX_X) lowTarget = MAX_X;
        slideTargetX = lowTarget;

        Serial.printf("[Tilt] slide first -> lowTarget=%d then climb=%d\n",
                      slideTargetX, climbTargetX);

        bool needFaceRight = !tiltHighSideIsRight;  // 面向低处
        if (needFaceRight != faceRight) {
            startTurn(needFaceRight, true);
            slideAfterTurn = true;
        } else {
            adultState = AdultState::SLIDE;
            stateTimer = 0;
            stateDuration = 0;
        }
    } else {
        // 小角度：转身朝高处，然后爬行或不动
        Serial.printf("[Tilt] climb/highSide -> target=%d\n", climbTargetX);

        bool needFaceRight = tiltHighSideIsRight;
        if (needFaceRight != faceRight) {
            startTurn(needFaceRight, true);
            climbAfterTurn = true;
        } else {
            startClimbOrIdle();
        }
    }
}

// 小角度倾斜：优先向高处缓慢爬行，偶尔原地休息
void TerrariumScene::startClimbOrIdle() {
    if (random(100) < 85) {
        Serial.printf("[Tilt] start CLIMB -> target=%d\n", climbTargetX);
        adultState = AdultState::CLIMB;
        stateTimer = 0;
        stateDuration = 0;
    } else {
        Serial.println("[Tilt] stay IDLE at high side");
        adultState = AdultState::IDLE;
        stateTimer = 0;
        stateDuration = random(40, 100);
    }
}

// 幼虫被戳：三段身体收缩 2 像素
void TerrariumScene::drawLarvaPoked(int x, int y, uint8_t palette) {
    uint16_t body = PixelRenderer::CREAM;
    uint16_t outline = PALETTE[palette][1];
    PixelRenderer::fillRect(x - 10, y - 2, 6, 6, body);
    PixelRenderer::fillRect(x - 2, y - 3, 6, 8, body);
    PixelRenderer::fillRect(x + 6, y - 2, 6, 6, body);
    PixelRenderer::fillRect(x - 12, y - 2, 2, 4, outline);
    PixelRenderer::fillRect(x + 10, y - 2, 2, 4, outline);
}

int TerrariumScene::getPokeTargetY() const {
    Bug& bug = GameEngine::ins().getBug();
    switch (bug.getStage()) {
        case Stage::EGG:
        case Stage::PUPA:
            return 95;
        case Stage::LARVA:
            return 100;
        case Stage::ADULT:
        default:
            return GROUND_Y - 20;
    }
}

void TerrariumScene::drawPokeAction() {
    if (!pokeReactionWasPoked || pokeReactionEndMs == 0) return;

    uint32_t now = Hal::ins().millis();
    if (now >= pokeReactionEndMs) return;

    uint32_t duration = pokeReactionEndMs - pokeReactionStartMs;
    if (duration == 0) return;

    uint32_t elapsed = now - pokeReactionStartMs;
    if (elapsed > duration) elapsed = duration;

    // 先伸入，再回收，形成一次明确的“戳”位移动画。
    const uint32_t pushDuration = (duration * 45) / 100;
    const int travel = 14;
    int advance;
    if (elapsed <= pushDuration || pushDuration == 0) {
        advance = (pushDuration == 0) ? travel : (int)((elapsed * travel) / pushDuration);
    } else {
        uint32_t retractElapsed = elapsed - pushDuration;
        uint32_t retractDuration = duration - pushDuration;
        advance = travel - (int)((retractElapsed * travel) / retractDuration);
    }
    if (advance < 0) advance = 0;
    if (advance > travel) advance = travel;

    uint8_t frameIndex = pokeFingerFrameIndex;
    if (frameIndex >= ActionAssets::FINGER_FRAME_COUNT) frameIndex = 0;
    uint16_t offset = pgm_read_word(&ActionAssets::FINGER_FRAMES[frameIndex].offset);
    uint16_t length = pgm_read_word(&ActionAssets::FINGER_FRAMES[frameIndex].length);

    static constexpr int CONTACT_OFFSET_X = 18;

    const int targetX = bugX + (pokeFingerFromRight ? CONTACT_OFFSET_X : -CONTACT_OFFSET_X);
    const int targetY = getPokeTargetY() + pokeFingerYOffset;
    uint8_t frameW = pgm_read_byte(&ActionAssets::FINGER_FRAMES[frameIndex].width);
    uint8_t frameH = pgm_read_byte(&ActionAssets::FINGER_FRAMES[frameIndex].height);
    uint8_t tipX = pgm_read_byte(&ActionAssets::FINGER_FRAMES[frameIndex].tipX);
    uint8_t tipY = pgm_read_byte(&ActionAssets::FINGER_FRAMES[frameIndex].tipY);
    if (pokeFingerFromRight) {
        tipX = frameW - 1 - tipX;
    }

    int x;
    if (pokeFingerFromRight) {
        x = targetX - tipX + travel - advance;
    } else {
        x = targetX - tipX - travel + advance;
    }
    int y = targetY - tipY;

    PixelRenderer::drawRgb565Rle(x, y,
                                 frameW,
                                 frameH,
                                 ActionAssets::FINGER_RLE,
                                 offset, length,
                                 pokeFingerFromRight);
}

// 戳冷却提示：甲虫上方闪烁小点
void TerrariumScene::drawPokeCooldownHint(int x, int y) {
    uint16_t c = ((Hal::ins().millis() / 100) % 2) ? PixelRenderer::GRAY : PixelRenderer::BLACK;
    int hintY = y - 16;
    if (hintY < 0) hintY = 0;
    PixelRenderer::fillRect(x - 2, hintY, 4, 4, c);
}
