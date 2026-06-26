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

