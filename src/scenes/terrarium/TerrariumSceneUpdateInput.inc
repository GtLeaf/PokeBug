SceneID TerrariumScene::update() {
    animFrame++;

    GameEngine& engine = GameEngine::ins();
    Bug& bug = GameEngine::ins().getBug();
    uint32_t nowMs = Hal::ins().millis();
    if (engine.takeDebugLarvaSubstrateFadeDue(nowMs)) {
        startLarvaSubstrateFade(nowMs);
    }
    if (substrateFadeActive && nowMs - substrateFadeStartMs >= SUBSTRATE_FADE_MS) {
        clearLarvaSubstrate();
    }
    if (BattleLink::ins().isBattlePeerSet()) {
        BattleLink::ins().update();
        if (BattleLink::ins().takeReceivedVisitRecall()) {
            engine.clearVisitSession();
            visitor.active = false;
            visitRecallConfirm = false;
            visitPingInFlight = false;
            Serial.println("[Terrarium] Visit recalled by peer");
        }
    }
    if (engine.isVisitGuest()) {
        updateVisitGuestLink(nowMs);
        bug.setSleeping(false);
        visitor.active = false;
        return nextScene;
    }
    if (!engine.hasActiveVisitSession()) {
        visitor.active = false;
        visitPingInFlight = false;
    } else if (engine.isVisitHost() && visitor.active) {
        updateVisitHostLink(nowMs);
        visitor.untilMs = nowMs + engine.getVisitRemainingMs();
    }

    if (bug.getStage() == Stage::ADULT) {
        bug.setSleeping(adultState == AdultState::REST);
    } else if (bug.getStage() == Stage::EGG && !bug.isDead()) {
        if (!substrateFadeActive) {
            updateLarvaSubstrate(nowMs);
        }
        bug.setSleeping(false);
    } else if (bug.getStage() == Stage::LARVA && !bug.isDead()) {
        if (!substrateFadeActive) {
            updateLarvaSubstrate(nowMs);
            updateLarvaState(bug, nowMs);
        } else {
            bug.setSleeping(false);
        }
    } else {
        larvaState = LarvaState::IDLE;
        observedLarvaEatGameMs = bug.getLastEatTime();
        bug.setSleeping(false);
        if (bug.getStage() != Stage::EGG && bug.getStage() != Stage::LARVA && !substrateFadeActive) {
            clearLarvaSubstrate();
        }
    }

    // 更新甲虫心智（成虫期且存活）。心智决策降频到每 10 帧一次，移动/动画仍保持 20fps。
    if (bug.getStage() == Stage::ADULT && !bug.isDead() &&
        (animFrame % AI_DECISION_INTERVAL_FRAMES) == 0) {
        mind.update(bug.getHunger(), bug.getMot(),
                    GameEngine::ins().isNight(), bug.isWoodPlaced(),
                    bug.hasFoodInTray() && bug.getFoodAmount() > 0,
                    Hal::ins().millis());
    }

    // 死亡后同时按 A+B 重置
    if (bug.isDead()) {
        bool a = Hal::ins().btnA_raw();
        bool b = Hal::ins().btnB_raw();
        if (a && b) {
            if (resetPressStart == 0) {
                bug.resetAfterDeath(GameEngine::ins().getGameNow());
                GameEngine::ins().clearTerrariumViewState();
                resetLocalViewState();
                resetPressStart = Hal::ins().millis();  // 标记已触发，防止连续重置
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

    updateVisitor(nowMs);

    // 戳反应计时
    if (pokeReactionEndMs != 0 && Hal::ins().millis() > pokeReactionEndMs) {
        pokeReactionEndMs = 0;
    }
    if (pokeThreatenEndMs != 0 && Hal::ins().millis() > pokeThreatenEndMs) {
        pokeThreatenEndMs = 0;
    }

    // shake 事件通知心智（GameEngine::processIMU() 已处理 bug.onShake，这里只做情绪反馈）
    if (bug.getStage() == Stage::ADULT && !bug.isDead()) {
        float mag = Hal::ins().getAccelMagnitude();
        if (mag > 2.0f) {
            uint32_t now = Hal::ins().millis();
            if (now - lastShakeNotifyMs > 500) {
                mind.onShaken(now);
                lastShakeNotifyMs = now;
            }
        }
    }

    // 成虫使用状态机自然移动：贴地、走走停停、有食物会靠近吃
    if (bug.getStage() == Stage::ADULT && !bug.isDead()) {
        updateAdultMovement();
    } else if (bug.getStage() == Stage::JUVENILE && !bug.isDead()) {
        updateJuvenileMovement();
    } else if (bug.getStage() != Stage::ADULT && bug.getStage() != Stage::LARVA) {
        bugX = 120;
        faceRight = true;
    }

    if (isMobileBeetleStage(bug.getStage()) && !bug.isDead()) {
        updateToyPhysics(Hal::ins().millis());
    }

    return nextScene;
}

void TerrariumScene::render() {
    Bug& bug = GameEngine::ins().getBug();

    drawStaticLayer();
    if (GameEngine::ins().isVisitGuest()) {
        drawVisitAwayOverlay();
        if (visitRecallConfirm) {
            drawVisitRecallConfirm();
        }
        return;
    }

    uint8_t substrateOpacity = 255;
    if (substrateFadeActive) {
        uint32_t fadeElapsed = Hal::ins().millis() - substrateFadeStartMs;
        if (fadeElapsed >= SUBSTRATE_FADE_MS) {
            substrateOpacity = 0;
        } else {
            substrateOpacity = (uint8_t)(255 - (fadeElapsed * 255UL) / SUBSTRATE_FADE_MS);
        }
    }
    drawLarvaSubstrate(substrateOpacity);
    drawBug();
    drawVisitor();
    if (isMobileBeetleStage(bug.getStage()) && !bug.isDead()) {
        drawToy();
    }
    drawToyEntry();
    drawPokeAction();
    drawCachedStatusBar();

    if (bug.isDead()) {
        drawDeathScreen();
    }
}

bool TerrariumScene::onButton(const ButtonEvent& ev) {
    Bug& bug = GameEngine::ins().getBug();
    if (bug.isDead()) return false;  // 死亡画面只接受 A+B 长按，由 update 处理

    if (GameEngine::ins().isVisitGuest()) {
        if (visitRecallConfirm) {
            if (ev.btn == 0 && ev.action == BtnAction::PRESSED) {
                if (!BattleLink::ins().isSending() &&
                    (BattleLink::ins().sendVisitRecall() || !BattleLink::ins().isBattlePeerSet())) {
                    GameEngine::ins().clearVisitSession();
                    visitRecallConfirm = false;
                    visitPingInFlight = false;
                    Serial.println("[Terrarium] Visit recall confirmed");
                }
                return true;
            }
            if (ev.btn == 1 && ev.action == BtnAction::PRESSED) {
                visitRecallConfirm = false;
                return true;
            }
            return true;
        }
        if (ev.btn == 0 && ev.action == BtnAction::LONG_PRESS) {
            nextScene = SCENE_MENU;
            return true;
        }
        if (ev.btn == 0 && ev.action == BtnAction::PRESSED) {
            return true;
        }
        if (ev.btn == 1 && ev.action == BtnAction::PRESSED) {
            visitRecallConfirm = true;
            return true;
        }
        if (ev.btn == 1) {
            return true;
        }
        return false;
    }

    if (ev.btn == 0 && ev.action == BtnAction::PRESSED) {
        uint64_t gameNow = GameEngine::ins().getGameNow();
        if (bug.getStage() == Stage::EGG) {
            // 卵期短按 A：投放底材；仍记录 A 次数参与气质判定。
            bug.onEggWater(gameNow);
            if (spawnLarvaSubstrate()) {
                Serial.println("[Terrarium] Dropped egg substrate");
            } else {
                Serial.println("[Terrarium] Drop egg substrate failed");
            }
        } else if (bug.getStage() == Stage::LARVA) {
            if (spawnLarvaSubstrate()) {
                Serial.println("[Terrarium] Dropped larva substrate");
            } else {
                Serial.println("[Terrarium] Drop larva substrate failed");
            }
        } else {
            // 短按 A：喂食
            if (bug.placeFoodInTray((FoodType)GameEngine::ins().getFoodStyle())) {
                Serial.println("[Terrarium] Fed bug");
            } else {
                Serial.println("[Terrarium] Feed failed");
            }
        }
        return true;
    }
    if (ev.btn == 1 && ev.action == BtnAction::PRESSED) {
        uint64_t gameNow = GameEngine::ins().getGameNow();
        uint32_t now = Hal::ins().millis();
        if (isMobileBeetleStage(bug.getStage()) &&
            startToyButtonInteraction(now, true)) {
            Serial.printf("[Terrarium] Toy interaction style=%u\n",
                          GameEngine::ins().getToyStyle());
            return true;
        }
        if (bug.getStage() == Stage::EGG) {
            // 卵期短按 B：戳蛋
            bug.onEggPoke(gameNow);
            startPokeFeedback(Hal::ins().millis(), 600, true);
            Serial.println("[Terrarium] Poked egg");
            return true;
        }
        // 其他阶段短按 B：戳甲虫
        bool targetVisitor = GameEngine::ins().isVisitHost() && chooseVisitorForHostInteraction();
        Serial.printf("[Terrarium] B poke targetVisitor=%u visitHost=%u visitorActive=%u falling=%u toyStyle=%u\n",
                      targetVisitor ? 1 : 0,
                      GameEngine::ins().isVisitHost() ? 1 : 0,
                      visitor.active ? 1 : 0,
                      visitor.falling ? 1 : 0,
                      GameEngine::ins().getToyStyle());
        if (targetVisitor) {
            startPokeFeedback(now, 600, true, true);
            reactVisitorToHostPoke(now);
            Serial.println("[Terrarium] Poked visitor bug");
            return true;
        }

        bool ok = bug.poke(gameNow);
        if (ok) {
            mind.onPoked(now);
            // 若已在 threaten 持续阶段（已播完动作、正 hold 最后一帧），只重置持续时间，
            // 不要从第 0 帧重新播放。
            bool inThreatenHold = (pokeThreatenEndMs != 0) &&
                                  (now >= pokeReactionStartMs + THREATEN_PLAY_MS);
            if (!inThreatenHold) {
                pokeReactionStartMs = now;
            }
            startPokeFeedback(now, 600, true);
            pokeThreatenEndMs = now + POKE_REACTION_MS;  // 威吓保持约 3s
            if (bug.getStage() == Stage::ADULT) {
                // 若从夜间休息中被戳醒，设置随机清醒冷却，不能马上重新入睡
                if (adultState == AdultState::REST) {
                    restResumeAllowedMs = now + random(REST_WAKEUP_COOLDOWN_MIN_MS,
                                                       REST_WAKEUP_COOLDOWN_MAX_MS + 1);
                }
                // 成虫朝向戳来的方向后退戒备
                faceRight = pokeFingerFromRight;
                turnTargetFaceRight = faceRight;
                int dir = pokeFingerFromRight ? -1 : 1;
                bugX += dir * 5;
                if (bugX < MIN_X) bugX = MIN_X;
                if (bugX > MAX_X) bugX = MAX_X;
                alertUntilMs = now + random(ALERT_MIN_MS, ALERT_MAX_MS + 1);
                adultState = AdultState::IDLE;
                stateTimer = 0;
                setIdleDuration();
            }
            Serial.println("[Terrarium] Poked bug");
        } else {
            // 冷却中：短提示
            startPokeFeedback(Hal::ins().millis(), 300, false);
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

