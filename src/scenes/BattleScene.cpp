#include "BattleScene.h"
#include "../core/GameEngine.h"
#include "../hardware/Hal.h"
#include "../hardware/PixelRenderer.h"
#include "../game/BattleCalc.h"

void BattleScene::onEnter() {
    state = State::CONNECTING;
    roundNum = 1;
    resultApplied = false;
    noOpponent = false;
    syncSent = false;
    roundSent = false;
    resultSent = false;
    meShakeEndMs = 0;
    enemyShakeEndMs = 0;
    initFromBug();
    BattleLink::ins().begin();
    BattleLink::ins().startDiscovery();
    stateStartMs = Hal::ins().millis();
}

void BattleScene::onExit() {
    BattleLink::ins().stopDiscovery();
    BattleLink::ins().end();
}

void BattleScene::initFromBug() {
    Bug& bug = GameEngine::ins().getBug();
    uint8_t hunger = bug.getHunger();
    int8_t hungerPenalty = (hunger < 10) ? -2 : 0;

    me.siz = (uint8_t)roundf(bug.getSiz());
    me.str = (uint8_t)roundf(bug.getStr()) + hungerPenalty;
    me.end = (uint8_t)roundf(bug.getEnd()) + hungerPenalty;
    me.spi = (uint8_t)roundf(bug.getSpi());
    me.mot = bug.getMot();
    me.palette = bug.getPaletteId();
    me.maxHp = BattleCalc::computeHp(me.siz, me.end);
    me.hp = me.maxHp;

    enemy = me;  // 占位，等待同步后覆盖
}

void BattleScene::buildSync() {
    Bug& bug = GameEngine::ins().getBug();
    battle_sync_t sync;
    sync.type = MSG_BATTLE_SYNC;
    sync.siz = (uint8_t)roundf(bug.getSiz());
    sync.str = (uint8_t)roundf(bug.getStr());
    sync.end = (uint8_t)roundf(bug.getEnd());
    sync.spi = (uint8_t)roundf(bug.getSpi());
    sync.motivation = bug.getMot();
    sync.hunger = bug.getHunger();
    sync.palette_id = bug.getPaletteId();
    BattleLink::ins().sendSync(sync);
}

SceneID BattleScene::update() {
    BattleLink::ins().update();

    switch (state) {
        case State::CONNECTING: {
            if (BattleLink::ins().isPeerConnected()) {
                state = State::SYNCING;
                syncSent = false;
                Serial.println("[BattleScene] peer connected, syncing");
            } else if (BattleLink::ins().getConnectState() == BattleLink::ConnectState::FAILED) {
                state = State::DONE;
                noOpponent = true;
                localWin = false;
            }
            break;
        }

        case State::SYNCING: {
            if (!syncSent) {
                buildSync();
                syncSent = true;
                stateStartMs = Hal::ins().millis();
                Serial.println("[BattleScene] sync sent, waiting opponent sync");
            }
            battle_sync_t sync;
            if (BattleLink::ins().takeReceivedSync(sync)) {
                enemy.siz = sync.siz;
                enemy.str = sync.str;
                enemy.end = sync.end;
                enemy.spi = sync.spi;
                enemy.mot = sync.motivation;
                enemy.palette = sync.palette_id;
                enemy.maxHp = BattleCalc::computeHp(enemy.siz, enemy.end);
                enemy.hp = enemy.maxHp;
                startRound();
                Serial.println("[BattleScene] sync done, round 1");
                break;
            }
            // 检测发送失败或超时
            if (syncSent && !BattleLink::ins().isSending()) {
                if (!BattleLink::ins().takeLastSendSuccess()) {
                    Serial.println("[BattleScene] sync send failed");
                }
            }
            if (Hal::ins().millis() - stateStartMs >= SYNC_TIMEOUT_MS) {
                Serial.println("[BattleScene] sync timeout");
                noOpponent = true;
                state = State::DONE;
            }
            break;
        }

        case State::ROUND_START: {
            startRound();
            break;
        }

        case State::CHARGE: {
            if (Hal::ins().millis() - stateStartMs >= CHARGE_MS) {
                state = State::CLASH;
                stateStartMs = Hal::ins().millis();
                roundSent = false;
                Serial.printf("[BattleScene] round %d charge -> clash\n", roundNum);
            }
            break;
        }

        case State::CLASH: {
            if (!roundSent) {
                computeClash();
                battle_round_t round;
                round.type = MSG_BATTLE_ROUND;
                round.round_num = (uint8_t)roundNum;
                round.my_dmg = (uint8_t)myDmg;
                round.my_hp = (uint8_t)me.hp;
                round.my_mot = me.mot;
                round.my_crit = myCrit ? 1 : 0;
                BattleLink::ins().sendRound(round);
                roundSent = true;
                stateStartMs = Hal::ins().millis();
                Serial.printf("[BattleScene] round %d clash sent, myDmg=%d\n", roundNum, myDmg);
            }

            battle_round_t enemyRound;
            if (BattleLink::ins().takeReceivedRound(enemyRound)) {
                enemyDmg = enemyRound.my_dmg;
                enemyCrit = (enemyRound.my_crit != 0);
                // 用对方汇报的 HP 更新显示
                enemy.hp = enemyRound.my_hp;
                enemy.mot = enemyRound.my_mot;
                applyRoundResult();
                state = State::ROUND_END;
                stateStartMs = Hal::ins().millis();
                break;
            }
            if (roundSent && !BattleLink::ins().isSending()) {
                if (!BattleLink::ins().takeLastSendSuccess()) {
                    Serial.printf("[BattleScene] round %d send failed\n", roundNum);
                }
            }
            if (Hal::ins().millis() - stateStartMs >= ROUND_TIMEOUT_MS) {
                Serial.printf("[BattleScene] round %d timeout\n", roundNum);
                noOpponent = true;
                state = State::DONE;
            }
            break;
        }

        case State::ROUND_END: {
            if (Hal::ins().millis() - stateStartMs >= ROUND_END_MS) {
                // 改为持续到一方 HP 归零，不再强制 3 回合结束
                if (me.hp <= 0 || enemy.hp <= 0) {
                    state = State::RESULT;
                    resultSent = false;
                    Serial.println("[BattleScene] round end -> result");
                } else {
                    roundNum++;
                    startRound();
                }
            }
            break;
        }

        case State::RESULT: {
            if (!resultSent) {
                computeAndSendResult();
                resultSent = true;
                stateStartMs = Hal::ins().millis();
                Serial.printf("[BattleScene] result sent, localWin=%d\n", localWin);
            }

            bool enemyWin = false;
            if (BattleLink::ins().takeReceivedResult(enemyWin)) {
                // 若双方结论不一致，以本地计算为准（按 HP 百分比）
                applyBattleResult();
                state = State::DONE;
                stateStartMs = Hal::ins().millis();
                Serial.println("[BattleScene] result received, battle done");
                break;
            }
            if (resultSent && !BattleLink::ins().isSending()) {
                if (!BattleLink::ins().takeLastSendSuccess()) {
                    Serial.println("[BattleScene] result send failed");
                }
            }
            if (Hal::ins().millis() - stateStartMs >= RESULT_TIMEOUT_MS) {
                Serial.println("[BattleScene] result timeout");
                applyBattleResult();
                state = State::DONE;
            }
            break;
        }

        case State::DONE: {
            // 等待按钮返回
            break;
        }
    }

    maybeLogStateStall();
    return nextScene;
}

void BattleScene::maybeLogStateStall() {
    uint32_t now = Hal::ins().millis();
    if (state != lastLoggedState || now - lastStateLogMs >= 2000) {
        Serial.printf("[BattleScene] state=%d round=%d peer=%d sendIdle=%d elapsed=%u\n",
                      (int)state, roundNum, BattleLink::ins().isPeerConnected(),
                      BattleLink::ins().isSendIdle(), now - stateStartMs);
        lastLoggedState = state;
        lastStateLogMs = now;
    }
}

void BattleScene::startRound() {
    state = State::CHARGE;
    stateStartMs = Hal::ins().millis();
    roundBoosted = false;
    myDmg = 0;
    enemyDmg = 0;
    myCrit = false;
    enemyCrit = false;
    roundSent = false;
    Serial.printf("[BattleScene] round %d start\n", roundNum);
}

void BattleScene::computeClash() {
    myDmg = BattleCalc::computeDamage(me.str, me.siz, enemy.end, me.spi, me.mot, myCrit);
    // 我方出招，敌方受击晃动
    if (myDmg > 0 && Hal::ins().millis() >= enemyShakeEndMs) {
        enemyShakeEndMs = Hal::ins().millis() + SHAKE_MS;
    }
}

void BattleScene::applyRoundResult() {
    me.hp -= enemyDmg;
    if (me.hp < 0) me.hp = 0;

    // 敌方出招，我方受击晃动
    if (enemyDmg > 0) {
        meShakeEndMs = Hal::ins().millis() + SHAKE_MS;
    }

    int motLoss = BattleCalc::computeMotLoss(me.spi);
    if (me.mot > motLoss) me.mot -= motLoss;
    else me.mot = 0;

    flashThisFrame = myCrit || enemyCrit;

    Serial.printf("[BattleScene] round %d: myDmg=%d enemyDmg=%d myHp=%d enemyHp=%d\n",
                  roundNum, myDmg, enemyDmg, me.hp, enemy.hp);
}

void BattleScene::computeAndSendResult() {
    // 判断胜负
    if (me.hp <= 0 && enemy.hp > 0) {
        localWin = false;
    } else if (enemy.hp <= 0 && me.hp > 0) {
        localWin = true;
    } else {
        // 按剩余 HP 百分比
        float myPct = (float)me.hp / me.maxHp;
        float enemyPct = (float)enemy.hp / enemy.maxHp;
        localWin = (myPct >= enemyPct);
    }
    BattleLink::ins().sendResult(localWin);
    Serial.printf("[BattleScene] result sent: %s\n", localWin ? "win" : "loss");
}

void BattleScene::applyBattleResult() {
    if (resultApplied) return;
    resultApplied = true;

    GameEngine::ins().getBug().onBattleEnd(localWin, GameEngine::ins().getGameNow());
    Serial.printf("[BattleScene] battle end, localWin=%d\n", localWin);
}

bool BattleScene::onButton(const ButtonEvent& ev) {
    if (ev.action == BtnAction::LONG_PRESS) {
        nextScene = SCENE_TERRARIUM;
        return true;
    }

    if (state == State::CHARGE && ev.action == BtnAction::PRESSED && ev.btn == 0) {
        if (!roundBoosted) {
            roundBoosted = true;
            me.mot += 15;
            if (me.mot > 100) me.mot = 100;
            Serial.println("[BattleScene] A pressed, MOT +15");
        }
        return true;
    }

    if (state == State::DONE && ev.action == BtnAction::PRESSED && ev.btn == 0) {
        nextScene = SCENE_TERRARIUM;
        return true;
    }

    return false;
}

void BattleScene::render() {
    switch (state) {
        case State::CONNECTING:
            drawConnecting();
            break;
        case State::DONE:
            drawResult();
            break;
        default:
            drawBattleField();
            break;
    }
}

void BattleScene::drawConnecting() {
    PixelRenderer::fillRect(0, 0, 240, 135, PixelRenderer::BLACK);
    PixelRenderer::drawPixelText(50, 50, "Searching...", PixelRenderer::WHITE, 2);

    uint32_t elapsed = Hal::ins().millis() - stateStartMs;
    int dots = (elapsed / 500) % 4;
    char buf[8];
    snprintf(buf, sizeof(buf), "%. *s", dots, "...");
    PixelRenderer::drawPixelText(100, 80, buf, PixelRenderer::WHITE, 1);
}

void BattleScene::drawBattleField() {
    PixelRenderer::fillRect(0, 0, 240, 135, PixelRenderer::rgb565(20, 20, 30));

    // 计算受击晃动偏移
    uint32_t now = Hal::ins().millis();
    auto computeShake = [](uint32_t nowMs, uint32_t endMs, int8_t amp, int8_t& outX, int8_t& outY) {
        if (nowMs >= endMs) { outX = 0; outY = 0; return; }
        outX = ((nowMs / 50) % 2) ? amp : -amp;
        outY = ((nowMs / 40) % 2) ? amp : -amp;
    };
    int8_t meOffX = 0, meOffY = 0;
    int8_t enemyOffX = 0, enemyOffY = 0;
    computeShake(now, meShakeEndMs, SHAKE_AMP, meOffX, meOffY);
    computeShake(now, enemyShakeEndMs, SHAKE_AMP, enemyOffX, enemyOffY);

    // 标题
    char buf[32];
    snprintf(buf, sizeof(buf), "ROUND %d", roundNum);
    PixelRenderer::drawPixelText(90, 5, buf, PixelRenderer::WHITE, 1);

    // 我方（左侧）
    PixelRenderer::fillRect(30 + meOffX, 35 + meOffY, 30, 20, me.palette == 0 ? PixelRenderer::BROWN : PixelRenderer::CREAM);
    snprintf(buf, sizeof(buf), "HP:%d/%d", me.hp, me.maxHp);
    PixelRenderer::drawPixelText(10, 60, buf, PixelRenderer::WHITE, 1);
    PixelRenderer::drawProgressBar(10, 72, 80, 6, (float)me.hp / me.maxHp,
                                   me.hp > me.maxHp / 2 ? PixelRenderer::GREEN : PixelRenderer::RED,
                                   PixelRenderer::GRAY);
    snprintf(buf, sizeof(buf), "MOT:%d", me.mot);
    PixelRenderer::drawPixelText(10, 82, buf, PixelRenderer::WHITE, 1);

    // 敌方（右侧）
    PixelRenderer::fillRect(180 + enemyOffX, 35 + enemyOffY, 30, 20, enemy.palette == 0 ? PixelRenderer::BROWN : PixelRenderer::CREAM);
    snprintf(buf, sizeof(buf), "HP:%d/%d", enemy.hp, enemy.maxHp);
    PixelRenderer::drawPixelText(150, 60, buf, PixelRenderer::WHITE, 1);
    PixelRenderer::drawProgressBar(150, 72, 80, 6, (float)enemy.hp / enemy.maxHp,
                                   enemy.hp > enemy.maxHp / 2 ? PixelRenderer::GREEN : PixelRenderer::RED,
                                   PixelRenderer::GRAY);

    // 中间状态提示
    const char* msg = "";
    switch (state) {
        case State::SYNCING: msg = "SYNC"; break;
        case State::CHARGE: msg = "CHARGE! A=boost"; break;
        case State::CLASH: msg = "CLASH!"; break;
        case State::ROUND_END: msg = myCrit ? "CRIT!" : (enemyCrit ? "OUCH!" : ""); break;
        default: break;
    }
    PixelRenderer::drawPixelText(80, 100, msg, PixelRenderer::YELLOW, 1);

    // 暴击闪屏（仅一帧）
    if (flashThisFrame) {
        PixelRenderer::fillRect(0, 0, 240, 135, PixelRenderer::WHITE);
        flashThisFrame = false;
    }
}

void BattleScene::drawResult() {
    PixelRenderer::fillRect(0, 0, 240, 135, PixelRenderer::BLACK);
    const char* resultText;
    uint16_t color;
    if (noOpponent) {
        resultText = "NO FOE";
        color = PixelRenderer::YELLOW;
    } else if (localWin) {
        resultText = "WIN!";
        color = PixelRenderer::GREEN;
    } else {
        resultText = "LOSE";
        color = PixelRenderer::RED;
    }
    PixelRenderer::drawPixelText(noOpponent ? 70 : 90, 45, resultText, color, 3);
    PixelRenderer::drawPixelText(70, 90, "Press A return", PixelRenderer::WHITE, 1);
}
