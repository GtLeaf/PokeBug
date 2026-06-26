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

