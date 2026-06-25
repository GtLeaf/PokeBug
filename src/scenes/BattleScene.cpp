#include "BattleScene.h"
#include "../core/GameEngine.h"
#include "../core/UiStrings.h"
#include "../hardware/Hal.h"
#include "../hardware/PixelRenderer.h"
#include "../game/BattleCalc.h"
#include "../assets/HerculesAdultSprites.h"

static uint8_t battleStatWithHunger(float value, uint8_t hunger) {
    int stat = (int)roundf(value);
    if (hunger < 10) stat -= 2;
    if (stat < 1) stat = 1;
    return (uint8_t)stat;
}

static uint16_t mixBattleRgb565(uint16_t base, uint16_t mix, float mixRatio) {
    if (mixRatio < 0.0f) mixRatio = 0.0f;
    if (mixRatio > 1.0f) mixRatio = 1.0f;
    uint8_t baseR = (base >> 11) & 0x1F;
    uint8_t baseG = (base >> 5) & 0x3F;
    uint8_t baseB = base & 0x1F;
    uint8_t mixR = (mix >> 11) & 0x1F;
    uint8_t mixG = (mix >> 5) & 0x3F;
    uint8_t mixB = mix & 0x1F;
    uint8_t r = (uint8_t)(baseR * (1.0f - mixRatio) + mixR * mixRatio);
    uint8_t g = (uint8_t)(baseG * (1.0f - mixRatio) + mixG * mixRatio);
    uint8_t b = (uint8_t)(baseB * (1.0f - mixRatio) + mixB * mixRatio);
    return (uint16_t)((r << 11) | (g << 5) | b);
}

static uint16_t brightenBattleRgb565(uint16_t color, float factor) {
    uint8_t r = (color >> 11) & 0x1F;
    uint8_t g = (color >> 5) & 0x3F;
    uint8_t b = color & 0x1F;
    uint16_t rr = (uint16_t)(r * factor);
    uint16_t gg = (uint16_t)(g * factor);
    uint16_t bb = (uint16_t)(b * factor);
    if (rr > 0x1F) rr = 0x1F;
    if (gg > 0x3F) gg = 0x3F;
    if (bb > 0x1F) bb = 0x1F;
    return (uint16_t)((rr << 11) | (gg << 5) | bb);
}

static uint16_t battleHueMain(Temperament temperament) {
    switch (temperament) {
        case Temperament::BRUTE:     return 0xF800; // 深红
        case Temperament::SWIFT:     return 0x6B7D; // 灰蓝
        case Temperament::GIANT:     return 0xFD20; // 橙褐
        case Temperament::RESILIENT: return 0xFE00; // 金色
        case Temperament::BALANCED:  return 0xFFFF; // 白/浅灰
        case Temperament::SPIRIT:    return 0x07E0; // 青绿
    }
    return 0xF800;
}

static uint16_t battleDepthColor(Temperament temperament, float ratio) {
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    uint16_t base = battleHueMain(temperament);
    if (ratio < 0.25f) {
        return mixBattleRgb565(mixBattleRgb565(base, PixelRenderer::GRAY, 0.5f), PixelRenderer::WHITE, 0.3f);
    }
    if (ratio < 0.50f) {
        return mixBattleRgb565(base, PixelRenderer::GRAY, 0.3f);
    }
    if (ratio < 0.75f) {
        return base;
    }
    return brightenBattleRgb565(base, 1.25f);
}

static uint8_t battlePaletteCode(Temperament temperament, float depth) {
    uint8_t t = (uint8_t)temperament;
    if (t > (uint8_t)Temperament::SPIRIT) t = (uint8_t)Temperament::SPIRIT;
    if (depth < 0.0f) depth = 0.0f;
    if (depth > 1.0f) depth = 1.0f;
    uint8_t bucket = (uint8_t)(depth * 4.0f);
    if (bucket > 3) bucket = 3;
    return (uint8_t)(0x80 | (bucket << 3) | t);
}

static uint16_t battlePaletteColor(uint8_t palette) {
    if (palette & 0x80) {
        uint8_t t = palette & 0x07;
        if (t > (uint8_t)Temperament::SPIRIT) t = (uint8_t)Temperament::SPIRIT;
        uint8_t bucket = (palette >> 3) & 0x03;
        float depth = (bucket + 0.5f) / 4.0f;
        return battleDepthColor((Temperament)t, depth);
    }

    // 旧 0-3 调色板 fallback，仅用于旧存档/NPC 随机色。
    switch (palette & 0x03) {
        case 1: return 0x07E0;
        case 2: return 0xFE00;
        case 3: return 0xE71C;
        case 0:
        default:
            return 0xF800;
    }
}

static float rollNpcBattleSpiReward(bool win, NpcData::Tier tier) {
    if (!win) return 0.0f;

    uint8_t chance = 0;
    uint8_t minTenths = 0;
    uint8_t maxTenths = 0;
    switch (tier) {
        case NpcData::Tier::ROOKIE:
            chance = 30; minTenths = 1; maxTenths = 1;
            break;
        case NpcData::Tier::NORMAL:
            chance = 45; minTenths = 1; maxTenths = 2;
            break;
        case NpcData::Tier::VETERAN:
            chance = 60; minTenths = 2; maxTenths = 3;
            break;
        case NpcData::Tier::LEGEND:
            chance = 80; minTenths = 3; maxTenths = 4;
            break;
        default:
            return 0.0f;
    }

    if ((uint8_t)random(100) >= chance) return 0.0f;
    return (float)random(minTenths, maxTenths + 1) * 0.15f;
}

void BattleScene::onEnter() {
    state = State::CONNECTING;
    roundNum = 1;
    resultApplied = false;
    noOpponent = false;
    syncSent = false;
    syncAcked = false;
    foeSyncReceived = false;
    roundSent = false;
    resultSent = false;
    roundComputed = false;
    motUpdatePending = false;
    motUpdateInFlight = false;
    motUpdateRound = 0;
    motUpdateValue = 0;
    meShakeEndMs = 0;
    enemyShakeEndMs = 0;
    firstAttackByMe = true;
    secondAttackPlanned = false;
    roundEndMyHp = 0;
    roundEndEnemyHp = 0;
    roundEndMyMot = 0;
    roundEndEnemyMot = 0;
    myGauge = 0.0f;
    enemyGauge = 0.0f;
    myRealGauge = 0.0f;
    enemyRealGauge = 0.0f;
    lastGaugeUpdateMs = 0;
    gaugeReadySinceMs = 0;
    rhythmWindowCount = 0;
    rhythmWindowIndex = 0;
    myAttackOpportunity = 1;
    rhythmPressedThisOpportunity = false;
    rhythmFeedback = RhythmFeedback::NONE;
    rhythmFeedbackUntilMs = 0;
    initFromBug();
    generateRhythmWindows();

    // 检查是否有本地 NPC 对战请求
    PendingNpcBattle& pending = GameEngine::ins().pendingNpcBattle();
    if (pending.active) {
        localNpcBattle = true;
        legendNpc = pending.legend;
        returnScene = pending.returnScene;
        enemy.siz = pending.siz;
        enemy.str = pending.str;
        enemy.end = pending.end;
        enemy.spi = pending.spi;
        enemy.spd = pending.spd;
        enemy.mot = pending.mot;
        enemy.palette = pending.palette;
        enemy.maxHp = BattleCalc::computeHp(enemy);
        enemy.hp = enemy.maxHp;
        state = State::ROUND_START;
        stateStartMs = Hal::ins().millis();
        Serial.printf("[BattleScene] Local NPC battle, return=%d legend=%d\n", returnScene, legendNpc);
    } else {
        localNpcBattle = false;
        legendNpc = false;
        returnScene = SCENE_TERRARIUM;
        BattleLink::ins().begin();
        stateStartMs = Hal::ins().millis();
    }
}

void BattleScene::onExit() {
    if (localNpcBattle) {
        GameEngine::ins().clearPendingNpcBattle();
    } else {
        BattleLink::ins().end();
    }
}

void BattleScene::initFromBug() {
    Bug& bug = GameEngine::ins().getBug();
    uint8_t hunger = bug.getHunger();

    me.siz = (uint8_t)roundf(bug.getSiz());
    me.str = battleStatWithHunger(bug.getStr(), hunger);
    me.end = battleStatWithHunger(bug.getEnd(), hunger);
    me.spi = (uint8_t)roundf(bug.getSpi());
    me.spd = (uint8_t)roundf(bug.getSpd());
    me.mot = bug.getMot();
    me.palette = battlePaletteCode(bug.getTemperament(), bug.getAdultDepth());
    me.maxHp = BattleCalc::computeHp(me);
    me.hp = me.maxHp;

    enemy = me;  // 占位，等待同步后覆盖
}

bool BattleScene::buildSync() {
    Bug& bug = GameEngine::ins().getBug();
    uint8_t hunger = bug.getHunger();
    battle_sync_t sync = {};
    sync.type = MSG_BATTLE_SYNC;
    sync.siz = (uint8_t)roundf(bug.getSiz());
    sync.str = battleStatWithHunger(bug.getStr(), hunger);
    sync.end = battleStatWithHunger(bug.getEnd(), hunger);
    sync.spi = (uint8_t)roundf(bug.getSpi());
    sync.spd = (uint8_t)roundf(bug.getSpd());
    sync.motivation = bug.getMot();
    sync.hunger = hunger;
    sync.palette_id = battlePaletteCode(bug.getTemperament(), bug.getAdultDepth());
    sync.visit_speed_x10 = 10;
    sync.str_cap = bug.getStrCap();
    sync.temperament = (uint8_t)bug.getTemperament();
    return BattleLink::ins().sendSync(sync);
}

SceneID BattleScene::update() {
    if (!localNpcBattle) {
        BattleLink::ins().update();
    }

    switch (state) {
        case State::CONNECTING: {
            if (BattleLink::ins().isBattlePeerSet()) {
                state = State::SYNCING;
                syncSent = false;
                Serial.println("[BattleScene] peer set by lobby, syncing");
            } else {
                Serial.println("[BattleScene] no battle peer -> NO FOE");
                noOpponent = true;
                localWin = false;
                state = State::DONE;
            }
            break;
        }

        case State::SYNCING: {
            if (!syncSent) {
                if (buildSync()) {
                    syncSent = true;
                    stateStartMs = Hal::ins().millis();
                    Serial.println("[BattleScene] sync sent, waiting opponent sync");
                } else {
                    Serial.println("[BattleScene] sync send failed, retry");
                }
            } else if (!syncAcked && !BattleLink::ins().isSending()) {
                if (BattleLink::ins().takeLastSendSuccess()) {
                    syncAcked = true;
                    Serial.println("[BattleScene] sync acked");
                } else {
                    syncSent = false;
                    Serial.println("[BattleScene] sync send failed, retry");
                }
            }

            if (!foeSyncReceived) {
                battle_sync_t sync;
                if (BattleLink::ins().takeReceivedSync(sync)) {
                    enemy.siz = sync.siz;
                    enemy.str = sync.str;
                    enemy.end = sync.end;
                    enemy.spi = sync.spi;
                    enemy.spd = sync.spd;
                    enemy.mot = sync.motivation;
                    enemy.palette = sync.palette_id;
                    enemy.maxHp = BattleCalc::computeHp(enemy);
                    enemy.hp = enemy.maxHp;
                    foeSyncReceived = true;
                    Serial.printf("[BattleScene] foe sync received, I am %s\n",
                                  BattleLink::ins().isHost() ? "HOST" : "CLIENT");
                }
            }

            if (syncAcked && foeSyncReceived) {
                Serial.printf("[BattleScene] sync done, I am %s\n",
                              BattleLink::ins().isHost() ? "HOST" : "CLIENT");
                startRound(true);
            }

            if (Hal::ins().millis() - stateStartMs >= SYNC_TIMEOUT_MS) {
                Serial.println("[BattleScene] sync timeout -> NO FOE");
                noOpponent = true;
                state = State::DONE;
            }
            break;
        }

        case State::ROUND_START: {
            startRound(true);
            break;
        }

        case State::GAUGE_FILLING: {
            uint32_t now = Hal::ins().millis();
            updateGauge(now);
            bool gaugeReady = (myRealGauge >= 1.0f || enemyRealGauge >= 1.0f);
            if (gaugeReady && gaugeReadySinceMs == 0) {
                gaugeReadySinceMs = now;
            }

            if (localNpcBattle) {
                // 本地 NPC 战：任意一方 gauge 充满即计算并执行攻击计划
                if (!roundComputed && tryComputeRound()) {
                    roundComputed = true;
                }
                if (roundComputed) {
                    beginAuthoritativeRound(hostRound);
                    roundSent = true;
                }
                break;
            }

            if (BattleLink::ins().isHost()) {
                // 主机：ATB 时机完全由主机推进；ready 只作为从机节奏输入后的 MOT 更新。
                battle_ready_t ready;
                if (BattleLink::ins().takeReceivedReady(ready)) {
                    if (ready.round_num == roundNum) {
                        enemy.mot = ready.my_mot;
                        Serial.printf("[BattleScene] host applied client rhythm mot=%d round=%d\n",
                                      enemy.mot, roundNum);
                    } else {
                        Serial.printf("[BattleScene] host ignored stale rhythm got=%d expected=%d\n",
                                      ready.round_num, roundNum);
                    }
                }

                if (!roundComputed && tryComputeRound()) {
                    roundComputed = true;
                    Serial.printf("[BattleScene] host round %d computed\n", roundNum);
                }

                // 主机已计算出权威行动，持续重试发送直到 ACK 成功
                if (roundComputed && !roundSent && !BattleLink::ins().isSending()) {
                    if (BattleLink::ins().takeLastSendSuccess()) {
                        beginAuthoritativeRound(hostRound);
                        roundSent = true;
                        Serial.printf("[BattleScene] host round %d sent and acked\n", roundNum);
                    } else if (BattleLink::ins().sendRound(hostRound)) {
                        Serial.printf("[BattleScene] host round %d send queued\n", roundNum);
                    } else {
                        Serial.println("[BattleScene] host sendRound failed, retry");
                    }
                }
            } else {
                // 从机：不再用本地 gauge 决定行动，只上报节奏输入后的 MOT 并等待主机 round。
                processMotUpdate();

                battle_round_t round;
                if (BattleLink::ins().takeReceivedRound(round)) {
                    if (round.round_num == roundNum) {
                        beginAuthoritativeRound(round);
                        Serial.printf("[BattleScene] client round %d applied\n", roundNum);
                    } else {
                        Serial.printf("[BattleScene] client round mismatch got=%d expected=%d\n",
                                      round.round_num, roundNum);
                    }
                    break;
                }
            }

            if (gaugeReadySinceMs != 0 && now - gaugeReadySinceMs >= ROUND_TIMEOUT_MS) {
                Serial.printf("[BattleScene] round %d timeout -> TIMEOUT\n", roundNum);
                computeLocalWin();
                state = State::TIMEOUT;
                stateStartMs = Hal::ins().millis();
            }
            break;
        }

        case State::ATTACK_ONE: {
            if (Hal::ins().millis() - stateStartMs >= ATTACK_MS) {
                bool attackByMe = isCurrentAttackByMe();
                applyCurrentAttack();
                resetGaugeAfterAction(attackByMe);

                if (secondAttackPlanned) {
                    state = State::ATTACK_TWO;
                    stateStartMs = Hal::ins().millis();
                } else {
                    me.hp = roundEndMyHp;
                    enemy.hp = roundEndEnemyHp;
                    me.mot = roundEndMyMot;
                    enemy.mot = roundEndEnemyMot;
                    state = State::ROUND_END;
                    stateStartMs = Hal::ins().millis();
                }
            }
            break;
        }

        case State::ATTACK_TWO: {
            if (Hal::ins().millis() - stateStartMs >= ATTACK_MS) {
                bool attackByMe = isCurrentAttackByMe();
                applyCurrentAttack();
                me.hp = roundEndMyHp;
                enemy.hp = roundEndEnemyHp;
                me.mot = roundEndMyMot;
                enemy.mot = roundEndEnemyMot;
                resetGaugeAfterAction(attackByMe);
                state = State::ROUND_END;
                stateStartMs = Hal::ins().millis();
            }
            break;
        }

        case State::ROUND_END: {
            if (Hal::ins().millis() - stateStartMs >= ROUND_END_MS) {
                if (me.hp <= 0 || enemy.hp <= 0 || roundNum >= MAX_ROUNDS) {
                    state = State::RESULT;
                    resultSent = false;
                    Serial.printf("[BattleScene] round end -> result (round=%d me.hp=%d enemy.hp=%d)\n",
                                  roundNum, me.hp, enemy.hp);
                } else {
                    roundNum++;
                    startRound(false);
                }
            }
            break;
        }

        case State::RESULT: {
            if (localNpcBattle) {
                // 本地 NPC 战：计算胜负后直接结算并进入 DONE
                if (!resultApplied) {
                    computeLocalWin();
                    applyBattleResult();
                }
                state = State::DONE;
                stateStartMs = Hal::ins().millis();
                break;
            }

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
                Serial.printf("[BattleScene] result received enemyWin=%d -> DONE\n", enemyWin);
                break;
            }
            if (resultSent && !BattleLink::ins().isSending()) {
                if (!BattleLink::ins().takeLastSendSuccess()) {
                    Serial.println("[BattleScene] result send failed");
                }
            }
            if (Hal::ins().millis() - stateStartMs >= RESULT_TIMEOUT_MS) {
                Serial.println("[BattleScene] result timeout -> TIMEOUT");
                computeLocalWin();
                state = State::TIMEOUT;
                stateStartMs = Hal::ins().millis();
            }
            break;
        }

        case State::TIMEOUT: {
            if (Hal::ins().millis() - stateStartMs >= TIMEOUT_NOTICE_MS) {
                applyBattleResult();
                state = State::DONE;
                stateStartMs = Hal::ins().millis();
                Serial.printf("[BattleScene] timeout notice done -> DONE localWin=%d\n", localWin);
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
                      (int)state, roundNum, BattleLink::ins().isBattlePeerSet(),
                      BattleLink::ins().isSendIdle(), now - stateStartMs);
        lastLoggedState = state;
        lastStateLogMs = now;
    }
}

void BattleScene::startRound(bool resetGauge) {
    state = State::GAUGE_FILLING;
    stateStartMs = Hal::ins().millis();
    lastGaugeUpdateMs = stateStartMs;
    gaugeReadySinceMs = 0;
    myDmg = 0;
    enemyDmg = 0;
    myCrit = false;
    enemyCrit = false;
    myAttackDodged = false;
    enemyAttackDodged = false;
    firstAttackByMe = true;
    secondAttackPlanned = false;
    roundEndMyHp = me.hp;
    roundEndEnemyHp = enemy.hp;
    roundEndMyMot = me.mot;
    roundEndEnemyMot = enemy.mot;
    roundSent = false;
    roundComputed = false;
    if (resetGauge) {
        myGauge = 0.0f;
        enemyGauge = 0.0f;
        myRealGauge = 0.0f;
        enemyRealGauge = 0.0f;
    }
    if (motUpdatePending && motUpdateRound != roundNum && !motUpdateInFlight) {
        motUpdatePending = false;
    }
    refreshRhythmWindow();
    Serial.printf("[BattleScene] round %d start\n", roundNum);
}

void BattleScene::updateGauge(uint32_t nowMs) {
    uint32_t delta = nowMs - lastGaugeUpdateMs;
    lastGaugeUpdateMs = nowMs;
    if (delta == 0) return;

    int myScore = tempoScore(me);
    int enemyScore = tempoScore(enemy);
    int fastestScore = myScore > enemyScore ? myScore : enemyScore;
    if (fastestScore < 1) fastestScore = 1;

    float fastestFillMs = (float)GAUGE_FILL_MS * GAUGE_BASE_SCORE / (float)fastestScore;
    if (fastestFillMs > (float)GAUGE_FASTEST_MAX_MS) {
        fastestFillMs = (float)GAUGE_FASTEST_MAX_MS;
    }

    float myDelta = (float)delta * ((float)myScore / (float)fastestScore) / fastestFillMs;
    float enemyDelta = (float)delta * ((float)enemyScore / (float)fastestScore) / fastestFillMs;
    myRealGauge += myDelta;
    enemyRealGauge += enemyDelta;
    if (myRealGauge > 1.0f) myRealGauge = 1.0f;
    if (enemyRealGauge > 1.0f) enemyRealGauge = 1.0f;

    // 显示 gauge 与真实触发 gauge 保持一致，避免到终点却不行动。
    myGauge = myRealGauge;
    enemyGauge = enemyRealGauge;
}

bool BattleScene::tryComputeRound() {
    // 任意一方真实 gauge 充满即可计算一次 ATB 行动。
    if (myRealGauge < 1.0f && enemyRealGauge < 1.0f) return false;
    if (roundComputed) return false;

    hostRound = computeAuthoritativeRound();
    return true;
}

void BattleScene::enterAttackOne() {
    state = State::ATTACK_ONE;
    stateStartMs = Hal::ins().millis();
    // 先攻方已经到达终点，后攻方保持当前 gauge 继续显示
    if (firstAttackByMe) myGauge = 1.0f;
    else enemyGauge = 1.0f;
}

void BattleScene::resetGaugeAfterAction(bool byMe) {
    if (byMe) {
        myGauge = 0.0f;
        myRealGauge = 0.0f;
    } else {
        enemyGauge = 0.0f;
        enemyRealGauge = 0.0f;
    }
}

void BattleScene::queueMotUpdate() {
    if (localNpcBattle || BattleLink::ins().isHost()) return;
    motUpdatePending = true;
    motUpdateInFlight = false;
    motUpdateRound = (uint8_t)roundNum;
    motUpdateValue = me.mot;
    processMotUpdate();
}

void BattleScene::processMotUpdate() {
    if (localNpcBattle || BattleLink::ins().isHost()) return;

    if (motUpdateInFlight && !BattleLink::ins().isSending()) {
        bool ok = BattleLink::ins().takeLastSendSuccess();
        motUpdateInFlight = false;
        if (ok) {
            motUpdatePending = false;
            Serial.printf("[BattleScene] client rhythm mot update acked round=%d mot=%d\n",
                          motUpdateRound, motUpdateValue);
            return;
        }
        Serial.println("[BattleScene] client rhythm mot update failed, retry");
    }

    if (!motUpdatePending || motUpdateInFlight || BattleLink::ins().isSending()) return;
    if (motUpdateRound != (uint8_t)roundNum) {
        motUpdatePending = false;
        return;
    }

    battle_ready_t ready;
    ready.type = MSG_BATTLE_READY;
    ready.round_num = motUpdateRound;
    ready.my_mot = motUpdateValue;
    if (BattleLink::ins().sendReady(ready)) {
        motUpdateInFlight = true;
        Serial.printf("[BattleScene] client rhythm mot update sent round=%d mot=%d\n",
                      motUpdateRound, motUpdateValue);
    }
}

void BattleScene::generateRhythmWindows() {
    rhythmWindowCount = 0;
    rhythmWindowIndex = 0;
    myAttackOpportunity = 1;
    rhythmPressedThisOpportunity = false;

    uint8_t opportunity = (uint8_t)random(RHYTHM_INTERVAL_MIN, RHYTHM_INTERVAL_MAX + 1);
    const int minStart = TEMPO_BAR_W * RHYTHM_ZONE_MIN_PCT / 100;
    const int maxEnd = TEMPO_BAR_W * RHYTHM_ZONE_MAX_PCT / 100;

    while (opportunity <= MAX_ROUNDS && rhythmWindowCount < 6) {
        uint8_t width = (uint8_t)random(RHYTHM_ZONE_W_MIN, RHYTHM_ZONE_W_MAX + 1);
        int maxStart = maxEnd - width;
        if (maxStart < minStart) maxStart = minStart;

        RhythmWindow& window = rhythmWindows[rhythmWindowCount++];
        window.opportunity = opportunity;
        window.widthPx = width;
        window.startPx = (uint8_t)random(minStart, maxStart + 1);

        opportunity = (uint8_t)(opportunity +
                                random(RHYTHM_INTERVAL_MIN, RHYTHM_INTERVAL_MAX + 1));
    }

    refreshRhythmWindow();
}

void BattleScene::refreshRhythmWindow() {
    while (rhythmWindowIndex < rhythmWindowCount &&
           rhythmWindows[rhythmWindowIndex].opportunity < myAttackOpportunity) {
        rhythmWindowIndex++;
    }
}

const BattleScene::RhythmWindow* BattleScene::currentRhythmWindow() const {
    if (rhythmWindowIndex >= rhythmWindowCount) return nullptr;
    const RhythmWindow& window = rhythmWindows[rhythmWindowIndex];
    return window.opportunity == myAttackOpportunity ? &window : nullptr;
}

bool BattleScene::isRhythmWindowActive() const {
    return currentRhythmWindow() != nullptr && !rhythmPressedThisOpportunity;
}

void BattleScene::showRhythmFeedback(RhythmFeedback feedback, uint32_t nowMs) {
    rhythmFeedback = feedback;
    rhythmFeedbackUntilMs = nowMs + RHYTHM_FEEDBACK_MS;
}

void BattleScene::handleRhythmButtonPress(uint32_t nowMs) {
    const RhythmWindow* window = currentRhythmWindow();
    if (window == nullptr || rhythmPressedThisOpportunity) return;

    rhythmPressedThisOpportunity = true;

    int markerPx = (int)(tempoProgress(true) * TEMPO_BAR_W + 0.5f);
    RhythmFeedback feedback = RhythmFeedback::MISS;
    uint8_t boost = RHYTHM_BOOST_MISS;
    int startPx = window->startPx;
    int endPx = startPx + window->widthPx;
    int centerPx = startPx + window->widthPx / 2;
    int perfectHalfPx = window->widthPx / 5;
    if (perfectHalfPx < 2) perfectHalfPx = 2;

    if (markerPx >= startPx && markerPx <= endPx) {
        int dist = markerPx - centerPx;
        if (dist < 0) dist = -dist;
        if (dist <= perfectHalfPx) {
            feedback = RhythmFeedback::PERFECT;
            boost = RHYTHM_BOOST_PERFECT;
        } else {
            feedback = RhythmFeedback::GREAT;
            boost = RHYTHM_BOOST_GREAT;
        }
    }

    uint16_t mot = (uint16_t)me.mot + boost;
    me.mot = mot > BATTLE_MOT_TEMP_CAP ? BATTLE_MOT_TEMP_CAP : (uint8_t)mot;
    queueMotUpdate();
    showRhythmFeedback(feedback, nowMs);
    Serial.printf("[BattleScene] rhythm %d opportunity=%d marker=%d zone=%d-%d mot+%d -> %d\n",
                  (int)feedback, myAttackOpportunity, markerPx, startPx, endPx, boost, me.mot);
}

void BattleScene::finishMyAttackOpportunity() {
    const RhythmWindow* window = currentRhythmWindow();
    if (window != nullptr) {
        rhythmWindowIndex++;
    }
    if (myAttackOpportunity < 255) {
        myAttackOpportunity++;
    }
    rhythmPressedThisOpportunity = false;
    refreshRhythmWindow();
}

battle_round_t BattleScene::computeAuthoritativeRound() {
    battle_round_t round;
    round.type = MSG_BATTLE_ROUND;
    round.round_num = (uint8_t)roundNum;

    bool hostCrit = false, clientCrit = false;
    bool hostDodged = false, clientDodged = false;
    round.host_dmg = 0;
    round.client_dmg = 0;
    round.host_gauge = (uint8_t)(myRealGauge * 100.0f + 0.5f);
    round.client_gauge = (uint8_t)(enemyRealGauge * 100.0f + 0.5f);
    if (round.host_gauge > 100) round.host_gauge = 100;
    if (round.client_gauge > 100) round.client_gauge = 100;

    // 行动判定：谁的 ATB gauge 先满谁行动；同时满时 SPD 高者优先。
    bool hostReady = myRealGauge >= 1.0f;
    bool clientReady = enemyRealGauge >= 1.0f;
    bool hostFirst = hostReady && (!clientReady || tempoScore(me) >= tempoScore(enemy));

    int hostHp = me.hp;
    int clientHp = enemy.hp;

    if (hostFirst) {
        BattleCalc::AttackResult hostAttack = BattleCalc::computeAttack(me, enemy);
        round.host_dmg = hostAttack.damage;
        hostCrit = hostAttack.crit;
        hostDodged = hostAttack.dodged;
        clientHp -= (int)round.host_dmg;
    } else {
        BattleCalc::AttackResult clientAttack = BattleCalc::computeAttack(enemy, me);
        round.client_dmg = clientAttack.damage;
        clientCrit = clientAttack.crit;
        clientDodged = clientAttack.dodged;
        hostHp -= (int)round.client_dmg;
    }

    if (hostHp < 0) hostHp = 0;
    if (clientHp < 0) clientHp = 0;
    round.host_hp = (uint8_t)hostHp;
    round.client_hp = (uint8_t)clientHp;

    int hostMotLoss = BattleCalc::computeMotLoss(me.spi, me.mot);
    int clientMotLoss = BattleCalc::computeMotLoss(enemy.spi, enemy.mot);
    int hostMot = (int)me.mot - hostMotLoss;
    int clientMot = (int)enemy.mot - clientMotLoss;
    if (hostMot < 0) hostMot = 0;
    if (clientMot < 0) clientMot = 0;
    if (hostMot > BATTLE_MOT_SOFT_CAP) hostMot = BATTLE_MOT_SOFT_CAP;
    if (clientMot > BATTLE_MOT_SOFT_CAP) clientMot = BATTLE_MOT_SOFT_CAP;
    round.host_mot = (uint8_t)hostMot;
    round.client_mot = (uint8_t)clientMot;

    round.crits = (hostCrit ? 0x01 : 0x00) |
                  (clientCrit ? 0x02 : 0x00) |
                  (hostDodged ? 0x04 : 0x00) |
                  (clientDodged ? 0x08 : 0x00) |
                  (hostFirst ? 0x10 : 0x00);

    myCrit = hostCrit;
    enemyCrit = clientCrit;
    myAttackDodged = hostDodged;
    enemyAttackDodged = clientDodged;
    myDmg = round.host_dmg;
    enemyDmg = round.client_dmg;

    return round;
}

void BattleScene::beginAuthoritativeRound(const battle_round_t& round) {
    // 本地 NPC 战没有经 BattleLink 设置 host 身份，统一按 host 视角（me=host, enemy=client）应用
    if (localNpcBattle || BattleLink::ins().isHost()) {
        roundEndMyHp = round.host_hp;
        roundEndEnemyHp = round.client_hp;
        roundEndMyMot = round.host_mot;
        roundEndEnemyMot = round.client_mot;
        myDmg = round.host_dmg;
        enemyDmg = round.client_dmg;
        myCrit = (round.crits & 0x01) != 0;
        enemyCrit = (round.crits & 0x02) != 0;
        myAttackDodged = (round.crits & 0x04) != 0;
        enemyAttackDodged = (round.crits & 0x08) != 0;
        firstAttackByMe = (round.crits & 0x10) != 0;
    } else {
        roundEndMyHp = round.client_hp;
        roundEndEnemyHp = round.host_hp;
        roundEndMyMot = round.client_mot;
        roundEndEnemyMot = round.host_mot;
        myDmg = round.client_dmg;
        enemyDmg = round.host_dmg;
        myCrit = (round.crits & 0x02) != 0;
        enemyCrit = (round.crits & 0x01) != 0;
        myAttackDodged = (round.crits & 0x08) != 0;
        enemyAttackDodged = (round.crits & 0x04) != 0;
        firstAttackByMe = (round.crits & 0x10) == 0;
    }

    float hostGauge = round.host_gauge / 100.0f;
    float clientGauge = round.client_gauge / 100.0f;
    if (localNpcBattle || BattleLink::ins().isHost()) {
        myRealGauge = myGauge = hostGauge;
        enemyRealGauge = enemyGauge = clientGauge;
    } else {
        myRealGauge = myGauge = clientGauge;
        enemyRealGauge = enemyGauge = hostGauge;
    }

    bool secondByMe = !firstAttackByMe;
    secondAttackPlanned = attackExistsFor(secondByMe);

    enterAttackOne();

    Serial.printf("[BattleScene] round %d applied: hostDmg=%d clientDmg=%d hostHp=%d clientHp=%d hGauge=%d cGauge=%d\n",
                  roundNum, round.host_dmg, round.client_dmg, round.host_hp, round.client_hp,
                  round.host_gauge, round.client_gauge);
}

bool BattleScene::attackExistsFor(bool byMe) const {
    return byMe ? (myDmg > 0 || myAttackDodged || myCrit)
                : (enemyDmg > 0 || enemyAttackDodged || enemyCrit);
}

bool BattleScene::isCurrentAttackByMe() const {
    if (state == State::ATTACK_ONE) return firstAttackByMe;
    if (state == State::ATTACK_TWO) return !firstAttackByMe;
    return false;
}

bool BattleScene::isCurrentAttackCritical() const {
    return isCurrentAttackByMe() ? myCrit : enemyCrit;
}

void BattleScene::applyCurrentAttack() {
    bool byMe = isCurrentAttackByMe();
    if (byMe) {
        if (myDmg > 0) {
            enemy.hp -= myDmg;
            if (enemy.hp < 0) enemy.hp = 0;
            enemyShakeEndMs = Hal::ins().millis() + SHAKE_MS;
        }
        if (myCrit) flashThisFrame = true;
        finishMyAttackOpportunity();
    } else {
        if (enemyDmg > 0) {
            me.hp -= enemyDmg;
            if (me.hp < 0) me.hp = 0;
            meShakeEndMs = Hal::ins().millis() + SHAKE_MS;
        }
        if (enemyCrit) flashThisFrame = true;
    }
}

void BattleScene::computeLocalWin() {
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
}

void BattleScene::computeAndSendResult() {
    computeLocalWin();
    BattleLink::ins().sendResult(localWin);
    Serial.printf("[BattleScene] result sent: %s\n", localWin ? "win" : "loss");
}

void BattleScene::applyBattleResult() {
    if (resultApplied) return;
    resultApplied = true;

    PendingNpcBattle& pending = GameEngine::ins().pendingNpcBattle();
    float spiReward = localNpcBattle ? rollNpcBattleSpiReward(localWin, pending.tier) : -1.0f;
    float spiGain = GameEngine::ins().getBug().onBattleEnd(localWin, GameEngine::ins().getGameNow(), spiReward);
    if (localNpcBattle) {
        GameEngine::ins().pendingNpcBattle().resultSet = true;
        GameEngine::ins().pendingNpcBattle().won = localWin;
        // 同时写入 GameEngine 的跨场景结果记录
        NpcBattleResult& res = GameEngine::ins().lastNpcBattleResult();
        res.valid = true;
        res.won = localWin;
        res.fromExplore = pending.fromExplore;
        res.fromCup = pending.fromCup;
        res.tier = pending.tier;
        res.legend = legendNpc;
        res.spiBoosted = spiGain > 0.001f;
    }
    Serial.printf("[BattleScene] battle end, localWin=%d\n", localWin);
}

bool BattleScene::onButton(const ButtonEvent& ev) {
    if (ev.action == BtnAction::LONG_PRESS) {
        Serial.printf("[BattleScene] long press btn=%d -> return scene %d\n", ev.btn, returnScene);
        nextScene = returnScene;
        return true;
    }

    if (state == State::GAUGE_FILLING && ev.action == BtnAction::DOWN && ev.btn == 0) {
        handleRhythmButtonPress(Hal::ins().millis());
        return true;
    }

    if (state == State::DONE && ev.action == BtnAction::PRESSED && ev.btn == 0) {
        nextScene = returnScene;
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
    PixelRenderer::drawPixelText(50, 50, UiStrings::BATTLE_SEARCHING, PixelRenderer::WHITE, 2);

    uint32_t elapsed = Hal::ins().millis() - stateStartMs;
    int dots = (elapsed / 500) % 4;
    char buf[8];
    snprintf(buf, sizeof(buf), "%.*s", dots, UiStrings::ELLIPSIS);
    PixelRenderer::drawPixelText(100, 80, buf, PixelRenderer::WHITE, 1);
}

void BattleScene::drawCombatantSprite(const Combatant& combatant, int centerX, int groundY,
                                      bool faceRight, int8_t shakeX, int8_t shakeY,
                                      bool attacking, bool critical) {
    const HerculesAdultSprites::RleFrame* frames = HerculesAdultSprites::WALK_FRAMES;
    const uint16_t* data = HerculesAdultSprites::WALK_RLE;
    uint8_t frameCount = HerculesAdultSprites::WALK_FRAME_COUNT;
    uint32_t durationMs = 1;
    uint32_t elapsed = Hal::ins().millis() - stateStartMs;

    if (attacking && elapsed < ATTACK_MS / 2) {
        frames = HerculesAdultSprites::ATTACK_DOWN_FRAMES;
        data = HerculesAdultSprites::ATTACK_DOWN_RLE;
        frameCount = HerculesAdultSprites::ATTACK_DOWN_FRAME_COUNT;
        durationMs = ATTACK_MS / 2;
        if (critical) {
            centerX += faceRight ? -3 : 3;
        }
    } else if (attacking) {
        frames = HerculesAdultSprites::ATTACK_UP_FRAMES;
        data = HerculesAdultSprites::ATTACK_UP_RLE;
        frameCount = HerculesAdultSprites::ATTACK_UP_FRAME_COUNT;
        uint32_t attackElapsed = elapsed - ATTACK_MS / 2;
        durationMs = ATTACK_MS / 2;
        uint32_t phase = attackElapsed < durationMs ? attackElapsed : durationMs;
        if (critical) {
            int lunge = (int)(phase * 14 / durationMs);
            centerX += faceRight ? lunge : -lunge;
        }
        elapsed = attackElapsed;
    }

    uint8_t frameIndex = 0;
    if (frameCount > 1) {
        if (elapsed > durationMs) elapsed = durationMs;
        frameIndex = (uint8_t)((elapsed * frameCount) / (durationMs + 1));
        if (frameIndex >= frameCount) frameIndex = frameCount - 1;
    }

    uint16_t offset = pgm_read_word(&frames[frameIndex].offset);
    uint16_t length = pgm_read_word(&frames[frameIndex].length);
    uint8_t w = pgm_read_byte(&frames[frameIndex].width);
    uint8_t h = pgm_read_byte(&frames[frameIndex].height);
    float scale = 1.0f;
    int drawW = (int)(w * scale);
    int drawH = (int)(h * scale);
    int drawX = centerX - drawW / 2 + shakeX;
    int drawY = groundY - drawH + shakeY;

    PixelRenderer::fillRect(centerX - 27 + shakeX, groundY + 2 + shakeY, 54, 3,
                            PixelRenderer::rgb565(24, 24, 28));
    uint16_t paletteColor = battlePaletteColor(combatant.palette);
    PixelRenderer::drawRgb565RleMappedScaled(drawX, drawY, w, h,
                                             data, offset, length, scale,
                                             HerculesAdultSprites::PALETTE_KEY, paletteColor,
                                             HerculesAdultSprites::PALETTE_KEY, paletteColor,
                                             HerculesAdultSprites::PALETTE_KEY, paletteColor,
                                             !faceRight);
}

int BattleScene::tempoScore(const Combatant& combatant) const {
    int score = TEMPO_BASE_SCORE + (int)combatant.spd * TEMPO_SPD_SLOPE;
    return score < 1 ? 1 : score;
}

float BattleScene::tempoProgress(bool forMe) const {
    return forMe ? myGauge : enemyGauge;
}

void BattleScene::drawTempoBar() {
    PixelRenderer::drawPixelText(TEMPO_LABEL_X, TEMPO_BAR_Y - 3,
                                 UiStrings::BATTLE_TEMPO, PixelRenderer::GRAY, 1);
    PixelRenderer::fillRect(TEMPO_BAR_X, TEMPO_BAR_Y, TEMPO_BAR_W, TEMPO_BAR_H,
                            PixelRenderer::rgb565(30, 30, 36));
    PixelRenderer::fillRect(TEMPO_BAR_X, TEMPO_BAR_Y + 2, TEMPO_BAR_W, 1,
                            PixelRenderer::rgb565(72, 72, 82));

    if (isRhythmWindowActive()) {
        const RhythmWindow* window = currentRhythmWindow();
        int zoneX = TEMPO_BAR_X + window->startPx;
        int perfectHalfPx = window->widthPx / 5;
        if (perfectHalfPx < 2) perfectHalfPx = 2;
        int centerX = zoneX + window->widthPx / 2;
        uint16_t barBase = PixelRenderer::rgb565(30, 30, 36);
        uint16_t zoneColor = mixBattleRgb565(barBase, PixelRenderer::ORANGE, 0.45f);
        uint16_t perfectColor = mixBattleRgb565(barBase, PixelRenderer::YELLOW, 0.58f);
        PixelRenderer::fillRect(zoneX, TEMPO_BAR_Y - 1, window->widthPx,
                                TEMPO_BAR_H + 2, zoneColor);
        PixelRenderer::fillRect(centerX - perfectHalfPx, TEMPO_BAR_Y - 1,
                                perfectHalfPx * 2 + 1, TEMPO_BAR_H + 2,
                                perfectColor);
    }

    PixelRenderer::fillRect(TEMPO_BAR_X + TEMPO_BAR_W, TEMPO_BAR_Y - 1, 2,
                            TEMPO_BAR_H + 2, PixelRenderer::CREAM);

    auto drawMarker = [&](bool mine, float progress, int yOffset) {
        if (progress < 0.0f) progress = 0.0f;
        if (progress > 1.0f) progress = 1.0f;
        int x = TEMPO_BAR_X + (int)(progress * TEMPO_BAR_W);
        int y = TEMPO_ICON_Y + yOffset;
        uint16_t color = mine ? PixelRenderer::CYAN : PixelRenderer::rgb565(230, 90, 100);
        PixelRenderer::fillRect(x - 3, y - 2, 7, 5, color);
        PixelRenderer::fillRect(x - 2, y - 1, 5, 3, brightenBattleRgb565(color, 1.25f));
    };

    drawMarker(true, tempoProgress(true), -2);
    drawMarker(false, tempoProgress(false), 4);
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
    snprintf(buf, sizeof(buf), "%s %d", UiStrings::BATTLE_ROUND, roundNum);
    PixelRenderer::drawPixelText(90, 5, buf, PixelRenderer::WHITE, 1);

    bool meAttacking = (state == State::ATTACK_ONE || state == State::ATTACK_TWO) &&
                       isCurrentAttackByMe();
    bool enemyAttacking = (state == State::ATTACK_ONE || state == State::ATTACK_TWO) &&
                          !isCurrentAttackByMe();
    bool currentAttackCritical = (state == State::ATTACK_ONE || state == State::ATTACK_TWO) &&
                                 isCurrentAttackCritical();

    // 我方（左侧）
    drawCombatantSprite(me, 58, 62, true, meOffX, meOffY,
                        meAttacking, meAttacking && currentAttackCritical);
    snprintf(buf, sizeof(buf), "%s:%d/%d", UiStrings::BATTLE_HP, me.hp, me.maxHp);
    PixelRenderer::drawPixelText(10, 60, buf, PixelRenderer::WHITE, 1);
    PixelRenderer::drawProgressBar(10, 72, 80, 6, (float)me.hp / me.maxHp,
                                   me.hp > me.maxHp / 2 ? PixelRenderer::GREEN : PixelRenderer::RED,
                                   PixelRenderer::GRAY);
    snprintf(buf, sizeof(buf), "%s:%d", UiStrings::BATTLE_MOT, me.mot);
    PixelRenderer::drawPixelText(10, 82, buf, PixelRenderer::WHITE, 1);

    // 敌方（右侧）
    drawCombatantSprite(enemy, 182, 62, false, enemyOffX, enemyOffY,
                        enemyAttacking, enemyAttacking && currentAttackCritical);
    snprintf(buf, sizeof(buf), "%s:%d/%d", UiStrings::BATTLE_HP, enemy.hp, enemy.maxHp);
    PixelRenderer::drawPixelText(150, 60, buf, PixelRenderer::WHITE, 1);
    PixelRenderer::drawProgressBar(150, 72, 80, 6, (float)enemy.hp / enemy.maxHp,
                                   enemy.hp > enemy.maxHp / 2 ? PixelRenderer::GREEN : PixelRenderer::RED,
                                   PixelRenderer::GRAY);

    // 中间状态提示
    const char* msg = "";
    uint16_t msgColor = PixelRenderer::YELLOW;
    float msgScale = 1;
    bool msgBold = false;
    int msgX = 80;
    int msgY = 100;
    if (now < rhythmFeedbackUntilMs && rhythmFeedback != RhythmFeedback::NONE) {
        switch (rhythmFeedback) {
            case RhythmFeedback::PERFECT:
                msg = UiStrings::BATTLE_FEEDBACK_MOT_PLUS_PLUS;
                msgColor = PixelRenderer::MAGENTA;
                break;
            case RhythmFeedback::GREAT:
                msg = UiStrings::BATTLE_FEEDBACK_MOT_PLUS;
                msgColor = PixelRenderer::CYAN;
                break;
            case RhythmFeedback::MISS:
                msg = UiStrings::BATTLE_FEEDBACK_MISS;
                msgColor = PixelRenderer::GRAY;
                break;
            case RhythmFeedback::NONE:
            default:
                break;
        }
    } else {
        switch (state) {
            case State::SYNCING: msg = UiStrings::BATTLE_STATE_SYNC; break;
            case State::GAUGE_FILLING:
                msg = isRhythmWindowActive() ? UiStrings::BATTLE_STATE_TIMING
                                             : UiStrings::BATTLE_STATE_CHARGE;
                break;
            case State::ATTACK_ONE:
            case State::ATTACK_TWO:
                msg = isCurrentAttackByMe() ? UiStrings::BATTLE_STATE_ATTACK
                                            : UiStrings::BATTLE_STATE_FOE_ATTACK;
                msgX = isCurrentAttackByMe() ? 34 : 154;
                msgY = 24;
                break;
            case State::ROUND_END:
                msg = myAttackDodged ? UiStrings::BATTLE_FEEDBACK_MISS :
                      (enemyAttackDodged ? UiStrings::BATTLE_STATE_DODGE :
                       (myCrit ? UiStrings::BATTLE_STATE_CRIT :
                        (enemyCrit ? UiStrings::BATTLE_STATE_OUCH : "")));
                break;
            case State::TIMEOUT:
                msg = UiStrings::BATTLE_STATE_TIME_OUT;
                msgColor = PixelRenderer::RED;
                msgScale = 2;
                msgBold = true;
                msgX = 64;
                msgY = 94;
                break;
            default: break;
        }
    }
    PixelRenderer::drawPixelText(msgX, msgY, msg, msgColor, msgScale);
    if (msgBold) {
        PixelRenderer::drawPixelText(msgX + 1, msgY, msg, msgColor, msgScale);
    }

    // 暴击闪屏（仅一帧）
    if (flashThisFrame) {
        PixelRenderer::fillRect(0, 0, 240, 135, PixelRenderer::WHITE);
        flashThisFrame = false;
    }

    drawTempoBar();
}

void BattleScene::drawResult() {
    PixelRenderer::fillRect(0, 0, 240, 135, PixelRenderer::BLACK);
    const char* resultText;
    uint16_t color;
    if (noOpponent) {
        resultText = UiStrings::BATTLE_RESULT_NO_FOE;
        color = PixelRenderer::YELLOW;
    } else if (localWin) {
        resultText = UiStrings::BATTLE_RESULT_WIN;
        color = PixelRenderer::GREEN;
    } else {
        resultText = UiStrings::BATTLE_RESULT_LOSE;
        color = PixelRenderer::RED;
    }
    PixelRenderer::drawPixelText(noOpponent ? 70 : 90, 45, resultText, color, 3);
    PixelRenderer::drawPixelText(70, 90, UiStrings::BATTLE_PRESS_A, PixelRenderer::WHITE, 1);
}
