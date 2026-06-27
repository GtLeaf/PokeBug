bool TerrariumScene::wantsToEat() {
    return mind.isDesiring(Desire::EAT);
}

void TerrariumScene::startTurn(bool targetFaceRight, bool continueWalking) {
    eatAfterTurn = false;
    beginActorTurn(localActor, targetFaceRight, continueWalking, 0, TURN_DURATION_FRAMES);
}

void TerrariumScene::startEatWhenFacingFood() {
    if (faceRight) {
        startTurn(false, false);
        eatAfterTurn = true;
        return;
    }
    enterEat();
}

// 开始向目标位置行走
void TerrariumScene::startWalkTo(int x, bool restTarget) {
    startWalkToDepth(x, TERRARIUM_DEPTH_INTERACTION_Z, restTarget);
}

void TerrariumScene::startWalkToDepth(int x, int z, bool restTarget) {
    bool wasOnWood = isAdultOnWoodRoute();
    beginActorWalkTo(localActor, x);
    if (restTarget && GameEngine::ins().getBug().isWoodPlaced()) {
        targetX = WOOD_REST_X;
    }
    targetZ = clampDepthZ(z);
    walkTargetIsRest = restTarget;
    woodDismountActive = wasOnWood &&
                         !restTarget &&
                         targetZ == TERRARIUM_DEPTH_INTERACTION_Z;
    if (TOY_DEBUG_LOGS &&
        restTarget &&
        GameEngine::ins().getBug().isWoodPlaced()) {
        Serial.printf("[WoodToy] rest-walk-start x=%d target=%d z=%u state=%s onWood=%u toyVisible=%u toyCharge=%u replay=%u noCatch=%ld\n",
                      bugX,
                      targetX,
                      bugZ,
                      adultStateName(adultState),
                      wasOnWood ? 1 : 0,
                      toyVisible ? 1 : 0,
                      toyCharging ? 1 : 0,
                      toyReplayReadyAfterImpact ? 1 : 0,
                      (long)(toyNoCatchUntilMs > Hal::ins().millis() ? toyNoCatchUntilMs - Hal::ins().millis() : 0));
    }
    if (WOOD_ADULT_DEBUG_LOGS &&
        GameEngine::ins().getBug().getStage() == Stage::ADULT &&
        GameEngine::ins().getBug().isWoodPlaced()) {
        Serial.printf("[WoodAdult] startWalk x=%d z=%u -> target=%d/%u rest=%u face=%u onWood=%u dismount=%u\n",
                      bugX, bugZ, targetX, targetZ,
                      walkTargetIsRest ? 1 : 0,
                      faceRight ? 1 : 0,
                      isAdultOnWoodRoute() ? 1 : 0,
                      woodDismountActive ? 1 : 0);
    }

    bool needFaceRight = (targetX > bugX);
    if (needFaceRight != faceRight) {
        startTurn(needFaceRight, true);
    } else {
        adultState = AdultState::WALK;
        stateTimer = 0;
        stateDuration = 0;   // 走到目的地为止
    }
}

void TerrariumScene::enterEat() {
    faceRight = false;
    adultState = AdultState::EAT;
    bugZ = TERRARIUM_DEPTH_INTERACTION_Z;
    targetZ = TERRARIUM_DEPTH_INTERACTION_Z;
    woodDismountActive = false;
    stateTimer = 0;
    stateDuration = random(EAT_DURATION_MIN_FRAMES, EAT_DURATION_MAX_FRAMES + 1);
    eatFrameInterval = (uint8_t)random(EAT_FRAME_INTERVAL_MIN, EAT_FRAME_INTERVAL_MAX + 1);
    eatBitesThisSession = 0;
    eatLastBiteMs = 0;
}

void TerrariumScene::enterRest() {
    adultState = AdultState::REST;
    bool woodPlaced = GameEngine::ins().getBug().isWoodPlaced();
    int beforeX = bugX;
    uint8_t beforeZ = bugZ;
    if (woodPlaced) {
        bugX = WOOD_REST_X;
    }
    bugZ = TERRARIUM_DEPTH_INTERACTION_Z;
    targetZ = TERRARIUM_DEPTH_INTERACTION_Z;
    stateTimer = 0;
    stateDuration = 0;
    walkTargetIsRest = false;
    woodDismountActive = false;
    if (!woodPlaced) {
        faceRight = random(2) == 0;
    }
    if (TOY_DEBUG_LOGS && woodPlaced) {
        Serial.printf("[WoodToy] enter-rest before=%d/%u after=%d/%u jump=%d toyVisible=%u toyCharge=%u replay=%u noCatch=%ld alert=%ld\n",
                      beforeX,
                      beforeZ,
                      bugX,
                      bugZ,
                      bugX - beforeX,
                      toyVisible ? 1 : 0,
                      toyCharging ? 1 : 0,
                      toyReplayReadyAfterImpact ? 1 : 0,
                      (long)(toyNoCatchUntilMs > Hal::ins().millis() ? toyNoCatchUntilMs - Hal::ins().millis() : 0),
                      (long)(alertUntilMs > Hal::ins().millis() ? alertUntilMs - Hal::ins().millis() : 0));
    }
    if (WOOD_ADULT_DEBUG_LOGS &&
        GameEngine::ins().getBug().getStage() == Stage::ADULT &&
        woodPlaced) {
        Serial.printf("[WoodAdult] enterRest x=%d z=%u foot=%d face=%u\n",
                      bugX, bugZ, adultFootY(), faceRight ? 1 : 0);
    }
}

void TerrariumScene::setIdleDuration() {
    if (GameEngine::ins().getBug().getStage() == Stage::JUVENILE) {
        stateDuration = random(18, 61);
        return;
    }
    bool alerted = alertUntilMs != 0 && Hal::ins().millis() < alertUntilMs;
    if (alerted) {
        stateDuration = random(100, 221);
    } else if (GameEngine::ins().isNight()) {
        stateDuration = random(50, 151);
    } else {
        stateDuration = random(100, 261);
    }
}

bool TerrariumScene::wantsToRestOnWood() {
    Bug& bug = GameEngine::ins().getBug();
    if (!bug.isWoodPlaced()) return false;
    if (WOOD_SLEEP_TEST_ALWAYS) return true;
    if (bug.getHunger() < 35) return false;
    return mind.isDesiring(Desire::REST);
}

bool TerrariumScene::shouldForceWoodSleepForTest() const {
    const Bug& bug = GameEngine::ins().getBug();
    return WOOD_SLEEP_TEST_ALWAYS &&
           bug.getStage() == Stage::ADULT &&
           bug.isWoodPlaced() &&
           !bug.isDead();
}

bool TerrariumScene::wantsToWander() {
    return mind.isDesiring(Desire::WANDER);
}

int TerrariumScene::pickRestTargetX() {
    Bug& bug = GameEngine::ins().getBug();
    if (bug.isWoodPlaced()) return WOOD_REST_X;

    int minX = MIN_X + 18;
    int maxX = MAX_X - 10;
    if (minX > maxX) {
        minX = MIN_X;
        maxX = MAX_X;
    }
    int x = random(minX, maxX + 1);
    if (abs(x - FOOD_X) < 28) {
        x = (x < FOOD_X) ? FOOD_X - 32 : FOOD_X + 32;
        if (x < minX) x = minX;
        if (x > maxX) x = maxX;
    }
    return x;
}

// 成虫状态机：贴地、走走停停、靠近食物进食
void TerrariumScene::updateAdultMovement() {
    Bug& bug = GameEngine::ins().getBug();

    stateTimer++;
    if (toyCharging) return;
    bool aiDecisionFrame = (stateTimer % AI_DECISION_INTERVAL_FRAMES) == 0;
    uint32_t nowMs = Hal::ins().millis();

    static bool woodLogInitialized = false;
    static AdultState lastWoodState = AdultState::IDLE;
    static int lastWoodX = 0;
    static uint8_t lastWoodZ = 0;
    static int lastWoodTargetX = 0;
    static uint8_t lastWoodTargetZ = 0;
    static bool lastWoodFaceRight = false;
    static bool lastWoodRoute = false;
    static bool lastWoodWalkRest = false;
    static bool lastWoodDismount = false;
    static bool lastWoodWalkAfterTurn = false;
    static bool lastWoodSlideAfterTurn = false;
    static bool lastWoodClimbAfterTurn = false;
    static uint32_t lastWoodLogMs = 0;

    if (WOOD_ADULT_DEBUG_LOGS &&
        bug.getStage() == Stage::ADULT &&
        bug.isWoodPlaced()) {
        bool onWoodRoute = isAdultOnWoodRoute();
        bool changed = !woodLogInitialized ||
                       adultState != lastWoodState ||
                       bugX != lastWoodX ||
                       bugZ != lastWoodZ ||
                       targetX != lastWoodTargetX ||
                       targetZ != lastWoodTargetZ ||
                       faceRight != lastWoodFaceRight ||
                       onWoodRoute != lastWoodRoute ||
                       walkTargetIsRest != lastWoodWalkRest ||
                       woodDismountActive != lastWoodDismount ||
                       walkAfterTurn != lastWoodWalkAfterTurn ||
                       slideAfterTurn != lastWoodSlideAfterTurn ||
                       climbAfterTurn != lastWoodClimbAfterTurn;
        if (changed || (onWoodRoute && nowMs - lastWoodLogMs >= 1500)) {
            Serial.printf("[WoodAdult] tick state=%s x=%d/%d z=%u/%u foot=%d face=%u onWood=%u rest=%u dismount=%u after(w/s/c)=%u/%u/%u timer=%lu/%lu resumeIn=%ld\n",
                          adultStateName(adultState),
                          bugX, targetX,
                          bugZ, targetZ,
                          adultFootY(),
                          faceRight ? 1 : 0,
                          onWoodRoute ? 1 : 0,
                          walkTargetIsRest ? 1 : 0,
                          woodDismountActive ? 1 : 0,
                          walkAfterTurn ? 1 : 0,
                          slideAfterTurn ? 1 : 0,
                          climbAfterTurn ? 1 : 0,
                          (unsigned long)stateTimer,
                          (unsigned long)stateDuration,
                          (long)(restResumeAllowedMs > nowMs ? restResumeAllowedMs - nowMs : 0));
            woodLogInitialized = true;
            lastWoodState = adultState;
            lastWoodX = bugX;
            lastWoodZ = bugZ;
            lastWoodTargetX = targetX;
            lastWoodTargetZ = targetZ;
            lastWoodFaceRight = faceRight;
            lastWoodRoute = onWoodRoute;
            lastWoodWalkRest = walkTargetIsRest;
            lastWoodDismount = woodDismountActive;
            lastWoodWalkAfterTurn = walkAfterTurn;
            lastWoodSlideAfterTurn = slideAfterTurn;
            lastWoodClimbAfterTurn = climbAfterTurn;
            lastWoodLogMs = nowMs;
        }
    }

    // threaten（威吓）期间不主动移动，但允许因大角度倾斜而滑落。
    if (pokeThreatenEndMs != 0 && nowMs < pokeThreatenEndMs) {
        if (adultState != AdultState::SLIDE) {
            return;
        }
    }

    switch (adultState) {
        case AdultState::IDLE:
            if (shouldForceWoodSleepForTest() &&
                Hal::ins().millis() >= restResumeAllowedMs) {
                int restTarget = pickRestTargetX();
                bool atRestTarget = bug.isWoodPlaced() ? bugX == restTarget
                                                       : abs(restTarget - bugX) <= 2;
                if (TOY_DEBUG_LOGS &&
                    (toyVisible || toyCharging || visitorToyCharging ||
                     toyReplayReadyAfterImpact || Hal::ins().millis() < toyNoCatchUntilMs)) {
                    Serial.printf("[WoodToy] force-rest state=%s x=%d target=%d at=%u toyVisible=%u localCharge=%u visitorCharge=%u replay=%u noCatch=%ld\n",
                                  adultStateName(adultState),
                                  bugX,
                                  restTarget,
                                  atRestTarget ? 1 : 0,
                                  toyVisible ? 1 : 0,
                                  toyCharging ? 1 : 0,
                                  visitorToyCharging ? 1 : 0,
                                  toyReplayReadyAfterImpact ? 1 : 0,
                                  (long)(toyNoCatchUntilMs > Hal::ins().millis() ? toyNoCatchUntilMs - Hal::ins().millis() : 0));
                }
                if (isAtInteractionDepth() && atRestTarget) {
                    enterRest();
                } else {
                    startWalkTo(restTarget, true);
                }
                break;
            }
            if (foodRefillGraceUntilMs != 0) {
                uint32_t nowMs = Hal::ins().millis();
                if (bug.getHunger() < EAT_CONTINUE_HUNGER &&
                    bug.hasFoodInTray() && bug.getFoodAmount() > 0 &&
                    isAtInteractionDepth() && abs(bugX - FOOD_X) <= 3) {
                    foodRefillGraceUntilMs = 0;
                    startEatWhenFacingFood();
                    break;
                }
                if (nowMs < foodRefillGraceUntilMs) {
                    faceRight = false;
                    break;
                }
                foodRefillGraceUntilMs = 0;
            }
            // 静止后段偶尔张望。AI 决策每 10 帧评估一次，概率按等效频率换算。
            if (aiDecisionFrame &&
                stateTimer > (stateDuration * 2) / 3 &&
                random(1000) < IDLE_LOOK_AROUND_CHANCE_PER_DECISION_1000) {
                startTurn(!faceRight, false);
                break;
            }
            // 静止结束后由 AI 心智决定下一步
            if (aiDecisionFrame && stateTimer >= stateDuration) {
                Desire d = mind.topDesire();
                static uint32_t lastVoiceLog = 0;
                if (Hal::ins().millis() - lastVoiceLog > 5000) {
                    Serial.printf("[Mind] %s | %s | \"%s\"\n",
                                  mind.desireName(), mind.moodName(), mind.innerVoice());
                    lastVoiceLog = Hal::ins().millis();
                }
                switch (d) {
                    case Desire::EAT:
                        if (bug.getHunger() < EAT_CONTINUE_HUNGER &&
                            bug.hasFoodInTray() && bug.getFoodAmount() > 0) {
                            startWalkTo(FOOD_X);
                        } else {
                            stateTimer = 0;
                            setIdleDuration();
                        }
                        break;
                    case Desire::REST:
                        if (Hal::ins().millis() >= restResumeAllowedMs) {
                            int restTarget = pickRestTargetX();
                            bool atRestTarget = bug.isWoodPlaced() ? bugX == restTarget
                                                                   : abs(restTarget - bugX) <= 2;
                            if (isAtInteractionDepth() && atRestTarget) {
                                enterRest();
                            } else {
                                startWalkTo(restTarget, true);
                            }
                        } else {
                            stateTimer = 0;
                            setIdleDuration();
                        }
                        break;
                    case Desire::WANDER: {
                        int newTarget = random(MIN_X, MAX_X + 1);
                        if (abs(newTarget - bugX) < 30) {
                            newTarget = (bugX < 120) ? random(140, MAX_X + 1)
                                                       : random(MIN_X, 100);
                        }
                        startWalkToDepth(newTarget, pickWanderTargetZ());
                        break;
                    }
                    case Desire::STARE:
                        // 发呆/张望：只偶尔转头，不主动移动。
                        if (random(100) < 35) {
                            startTurn(!faceRight, false);
                        } else {
                            stateTimer = 0;
                            setIdleDuration();
                        }
                        break;
                    case Desire::HIDE: {
                        // 警戒/躲避：后退一小段，避开刚才面向的刺激方向。
                        int hideTarget = bugX + (faceRight ? -28 : 28);
                        if (hideTarget < MIN_X) hideTarget = MIN_X;
                        if (hideTarget > MAX_X) hideTarget = MAX_X;
                        if (abs(hideTarget - bugX) > 2) {
                            startWalkTo(hideTarget);
                        } else {
                            stateTimer = 0;
                            setIdleDuration();
                        }
                        break;
                    }
                    default:
                        stateTimer = 0;
                        setIdleDuration();
                        break;
                }
                mind.resetActivityTimer(Hal::ins().millis());
            }
            break;

        case AdultState::TURN:
            // 转身动画完成后朝向已改变，进入对应后续状态
            if (stateTimer >= stateDuration) {
                localActor.turning = false;
                faceRight = turnTargetFaceRight;
                stateTimer = 0;
                bool doSlide = slideAfterTurn;
                bool doClimb = climbAfterTurn;
                bool doWalk = walkAfterTurn;
                bool doEat = eatAfterTurn;
                slideAfterTurn = false;
                climbAfterTurn = false;
                walkAfterTurn = false;
                eatAfterTurn = false;
                if (doSlide) {
                    woodDismountActive = false;
                    adultState = AdultState::SLIDE;
                    stateDuration = 0;
                } else if (doClimb) {
                    woodDismountActive = false;
                    adultState = AdultState::CLIMB;
                    stateDuration = 0;
                } else if (doWalk) {
                    adultState = AdultState::WALK;
                    stateDuration = 0;
                } else if (doEat) {
                    enterEat();
                } else {
                    woodDismountActive = false;
                    adultState = AdultState::IDLE;
                    setIdleDuration();
                }
            }
            break;

        case AdultState::WALK:
            {
                int dx = 0;
                if (abs(targetX - bugX) > 2) {
                    dx = (targetX > bugX) ? 1 : -1;
                    faceRight = (dx > 0);
                }
                bool walkingToFood = (targetX == FOOD_X && targetZ == TERRARIUM_DEPTH_INTERACTION_Z);

                // 每 3 帧移动 1 像素，模拟甲虫缓慢爬行；进食心切时走快点
                uint8_t stepInterval = (walkingToFood && mind.isDesiring(Desire::EAT)) ? 2 : 3;
                if (stateTimer % stepInterval == 0) {
                    if (dx != 0) {
                        bugX += dx;
                        if (bugX < MIN_X) bugX = MIN_X;
                        int maxWalkX = ((walkTargetIsRest || woodDismountActive) && bug.isWoodPlaced())
                                       ? WOOD_REST_X
                                       : MAX_X;
                        if (bugX > maxWalkX) bugX = maxWalkX;
                    }
                    if (bugZ < targetZ) {
                        bugZ++;
                    } else if (bugZ > targetZ) {
                        bugZ--;
                    }
                    if (woodDismountActive &&
                        (bugX <= WOOD_SURFACE_LEFT_X - WOOD_DISMOUNT_RAMP_PX ||
                         bugX >= WOOD_SURFACE_RIGHT_X + WOOD_DISMOUNT_RAMP_PX ||
                         bugZ != TERRARIUM_DEPTH_INTERACTION_Z)) {
                        woodDismountActive = false;
                    }
                }

                // 到达目标
                bool reachedX = walkTargetIsRest ? bugX == targetX : abs(targetX - bugX) <= 2;
                if (reachedX && bugZ == targetZ) {
                    if (targetX == FOOD_X && isAtInteractionDepth() &&
                        bug.getHunger() < EAT_CONTINUE_HUNGER &&
                        bug.hasFoodInTray() && bug.getFoodAmount() > 0) {
                        startEatWhenFacingFood();
                    } else if (walkTargetIsRest && isAtInteractionDepth()) {
                        enterRest();
                    } else {
                        walkTargetIsRest = false;
                        woodDismountActive = false;
                        adultState = AdultState::IDLE;
                        stateTimer = 0;
                        setIdleDuration();
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
                    setIdleDuration();
                }
            }
            break;

        case AdultState::EAT:
            {
            // 面向食物盘（食物在左侧，所以朝左）
            faceRight = false;
            uint32_t nowMs = Hal::ins().millis();
            bool firstBite = eatBitesThisSession == 0;
            bool biteDue = firstBite || (nowMs - eatLastBiteMs >= EAT_BITE_INTERVAL_MS);
            if (biteDue && bug.getHunger() < EAT_CONTINUE_HUNGER) {
                // 成虫进食节奏用现实时间控制；forceBite 只绕过 Bug 内部的虚拟时间间隔。
                if (bug.eatFromTray(GameEngine::ins().getGameNow(), true)) {
                    eatBitesThisSession++;
                    eatLastBiteMs = nowMs;
                }
            }
            // 多口之间保持同一个 EAT session，不重置动画；饱足后也至少咀嚼一小段再离开。
            bool foodGone = !bug.hasFoodInTray() || bug.getFoodAmount() == 0;
            bool chewedEnough = stateTimer >= EAT_MIN_EXIT_FRAMES;
            bool satisfied = eatBitesThisSession > 0 &&
                             bug.getHunger() >= EAT_CONTINUE_HUNGER &&
                             chewedEnough;
            bool timedOut = stateTimer >= stateDuration;
            if (foodGone || satisfied || timedOut) {
                mind.onAte(Hal::ins().millis());
                if (foodGone && bug.getHunger() < EAT_CONTINUE_HUNGER) {
                    foodRefillGraceUntilMs = Hal::ins().millis() + FOOD_REFILL_GRACE_MS;
                } else {
                    foodRefillGraceUntilMs = 0;
                }
                adultState = AdultState::IDLE;
                stateTimer = 0;
                setIdleDuration();
            }
            }
            break;

        case AdultState::REST:
            // 休息/睡觉：有腐木时会优先趴在腐木旁；没有腐木时也会找一处安静位置睡。
            {
                bool alerted = alertUntilMs != 0 && Hal::ins().millis() < alertUntilMs;
                if ((!WOOD_SLEEP_TEST_ALWAYS && bug.getHunger() < 35) || alerted) {
                    adultState = AdultState::IDLE;
                    stateTimer = 0;
                    setIdleDuration();
                    break;
                }
                if (bug.isWoodPlaced() && abs(WOOD_REST_X - bugX) <= 2) {
                    bug.recordWoodRest(GameEngine::ins().getGameNow());
                }
                if (aiDecisionFrame &&
                    !GameEngine::ins().isNight() &&
                    stateTimer > 180 &&
                    random(1000) < REST_WAKE_CHANCE_PER_DECISION_1000) {
                    restResumeAllowedMs = Hal::ins().millis() +
                                          random(REST_WAKEUP_COOLDOWN_MIN_MS,
                                                 REST_WAKEUP_COOLDOWN_MAX_MS + 1);
                    adultState = AdultState::IDLE;
                    stateTimer = 0;
                    setIdleDuration();
                }
            }
            break;

        case AdultState::THREATEN:
        case AdultState::ATTACK_DOWN:
        case AdultState::ATTACK_UP:
            adultState = AdultState::IDLE;
            stateTimer = 0;
            setIdleDuration();
            break;
    }
}

void TerrariumScene::updateJuvenileMovement() {
    stateTimer++;
    bugZ = TERRARIUM_DEPTH_INTERACTION_Z;
    targetZ = TERRARIUM_DEPTH_INTERACTION_Z;
    if (toyCharging) return;
    bool aiDecisionFrame = (stateTimer % AI_DECISION_INTERVAL_FRAMES) == 0;

    switch (adultState) {
        case AdultState::IDLE:
            if (aiDecisionFrame &&
                stateTimer > (stateDuration / 2) &&
                random(1000) < JUVENILE_LOOK_AROUND_CHANCE_PER_DECISION_1000) {
                startTurn(!faceRight, false);
                break;
            }
            if (aiDecisionFrame && stateTimer >= stateDuration) {
                int newTarget = random(MIN_X, MAX_X + 1);
                if (abs(newTarget - bugX) < 24) {
                    if (bugX < 120) {
                        newTarget = random(135, MAX_X + 1);
                    } else {
                        newTarget = random(MIN_X, 106);
                    }
                }
                startWalkTo(newTarget);
            }
            break;

        case AdultState::TURN:
            if (stateTimer >= stateDuration) {
                localActor.turning = false;
                faceRight = turnTargetFaceRight;
                stateTimer = 0;
                bool doWalk = walkAfterTurn;
                walkAfterTurn = false;
                if (doWalk) {
                    adultState = AdultState::WALK;
                    stateDuration = 0;
                } else {
                    adultState = AdultState::IDLE;
                    setIdleDuration();
                }
                slideAfterTurn = false;
                climbAfterTurn = false;
            }
            break;

        case AdultState::WALK:
            {
                int dx = (targetX > bugX) ? 1 : -1;
                faceRight = dx > 0;
                // 青年期睡得少、爱动，移动比成虫更频繁一点。
                if (stateTimer % 2 == 0) {
                    bugX += dx;
                    if (bugX < MIN_X) bugX = MIN_X;
                    if (bugX > MAX_X) bugX = MAX_X;
                }
                if (abs(targetX - bugX) <= 2) {
                    adultState = AdultState::IDLE;
                    stateTimer = 0;
                    walkTargetIsRest = false;
                    setIdleDuration();
                }
            }
            break;

        case AdultState::EAT:
        case AdultState::REST:
        case AdultState::SLIDE:
        case AdultState::CLIMB:
        case AdultState::THREATEN:
        case AdultState::ATTACK_DOWN:
        case AdultState::ATTACK_UP:
        default:
            adultState = AdultState::IDLE;
            stateTimer = 0;
            walkTargetIsRest = false;
            setIdleDuration();
            break;
    }
}

void TerrariumScene::drawFoodTray() {
    Bug& bug = GameEngine::ins().getBug();
    Stage stage = bug.getStage();
    if (stage != Stage::JUVENILE && stage != Stage::ADULT) return;
    if (GameEngine::ins().getBowlStyle() == 0xFF) return;

    static constexpr int BOWL_X = 14;
    static constexpr int BOWL_Y = GROUND_Y - BowlAssets::FRAME_H + 8;

    uint8_t style = GameEngine::ins().getBowlStyle();
    if (style >= BowlAssets::SPRITE_COUNT) style = 0;
    uint16_t bowlOffset = pgm_read_word(&BowlAssets::SPRITE_FRAMES[style].offset);
    uint16_t bowlLength = pgm_read_word(&BowlAssets::SPRITE_FRAMES[style].length);
    PixelRenderer::drawRgb565Rle(BOWL_X, BOWL_Y,
                                 BowlAssets::FRAME_W,
                                 BowlAssets::FRAME_H,
                                 BowlAssets::SPRITE_RLE,
                                 bowlOffset, bowlLength);

    if (bug.hasFoodInTray() && bug.getFoodAmount() > 0) {
        uint8_t foodStyle = (uint8_t)bug.getFoodInTrayType();
        if (foodStyle >= FoodAssets::SPRITE_COUNT) foodStyle = 0;
        uint16_t foodOffset = pgm_read_word(&FoodAssets::SPRITE_FRAMES[foodStyle].offset);
        uint16_t foodLength = pgm_read_word(&FoodAssets::SPRITE_FRAMES[foodStyle].length);
        PixelRenderer::drawRgb565RleEaten(BOWL_X + (BowlAssets::FRAME_W - FoodAssets::FRAME_W) / 2,
                                          BOWL_Y - 3,
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
    Stage stage = bug.getStage();
    if (stage != Stage::JUVENILE && stage != Stage::ADULT) return;
    if (!bug.isWoodPlaced()) return;

    uint8_t style = GameEngine::ins().getWoodStyle();
    if (style >= WoodAssets::SPRITE_COUNT) style = 0;
    uint16_t offset = pgm_read_word(&WoodAssets::SPRITE_FRAMES[style].offset);
    uint16_t length = pgm_read_word(&WoodAssets::SPRITE_FRAMES[style].length);
    PixelRenderer::drawRgb565Rle(WOOD_DRAW_RIGHT_X - WoodAssets::FRAME_W,
                                 GROUND_Y - WoodAssets::FRAME_H + 6,
                                 WoodAssets::FRAME_W,
                                 WoodAssets::FRAME_H,
                                 WoodAssets::SPRITE_RLE,
                                 offset, length);
}
