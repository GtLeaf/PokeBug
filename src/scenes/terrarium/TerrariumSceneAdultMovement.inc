bool TerrariumScene::wantsToEat() {
    return mind.isDesiring(Desire::EAT);
}

void TerrariumScene::startTurn(bool targetFaceRight, bool continueWalking) {
    beginActorTurn(localActor, targetFaceRight, continueWalking, 0, TURN_DURATION_FRAMES);
}

// 开始向目标位置行走
void TerrariumScene::startWalkTo(int x, bool restTarget) {
    beginActorWalkTo(localActor, x);
    walkTargetIsRest = restTarget;

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
    adultState = AdultState::EAT;
    stateTimer = 0;
    stateDuration = random(EAT_DURATION_MIN_FRAMES, EAT_DURATION_MAX_FRAMES + 1);
    eatFrameInterval = (uint8_t)random(EAT_FRAME_INTERVAL_MIN, EAT_FRAME_INTERVAL_MAX + 1);
    eatBitesThisSession = 0;
    eatLastBiteMs = 0;
}

void TerrariumScene::enterRest() {
    adultState = AdultState::REST;
    stateTimer = 0;
    stateDuration = 0;
    walkTargetIsRest = false;
    faceRight = random(2) == 0;
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
    if (bug.getHunger() < 35) return false;
    return mind.isDesiring(Desire::REST);
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

    // threaten（威吓）期间不主动移动，但允许因大角度倾斜而滑落。
    if (pokeThreatenEndMs != 0 && Hal::ins().millis() < pokeThreatenEndMs) {
        if (adultState != AdultState::SLIDE) {
            return;
        }
    }

    switch (adultState) {
        case AdultState::IDLE:
            if (foodRefillGraceUntilMs != 0) {
                uint32_t nowMs = Hal::ins().millis();
                if (bug.getHunger() < EAT_CONTINUE_HUNGER &&
                    bug.hasFoodInTray() && bug.getFoodAmount() > 0 && abs(bugX - FOOD_X) <= 3) {
                    foodRefillGraceUntilMs = 0;
                    enterEat();
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
                            if (abs(restTarget - bugX) <= 2) {
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
                        startWalkTo(newTarget);
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
                    setIdleDuration();
                }
            }
            break;

        case AdultState::WALK:
            {
                int dx = (targetX > bugX) ? 1 : -1;
                faceRight = (dx > 0);
                bool walkingToFood = (targetX == FOOD_X);

                // 每 3 帧移动 1 像素，模拟甲虫缓慢爬行；进食心切时走快点
                uint8_t stepInterval = (walkingToFood && mind.isDesiring(Desire::EAT)) ? 2 : 3;
                if (stateTimer % stepInterval == 0) {
                    bugX += dx;
                    if (bugX < MIN_X) bugX = MIN_X;
                    if (bugX > MAX_X) bugX = MAX_X;
                }

                // 到达目标
                if (abs(targetX - bugX) <= 2) {
                    if (targetX == FOOD_X && bug.getHunger() < EAT_CONTINUE_HUNGER &&
                        bug.hasFoodInTray() && bug.getFoodAmount() > 0) {
                        enterEat();
                    } else if (walkTargetIsRest) {
                        enterRest();
                    } else {
                        walkTargetIsRest = false;
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
                if (bug.getHunger() < 35 || alerted) {
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
                if (walkAfterTurn) {
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
    if (stage == Stage::EGG || stage == Stage::LARVA || stage == Stage::PUPA) return;
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
        uint8_t foodStyle = GameEngine::ins().getFoodStyle();
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
    if (stage == Stage::EGG || stage == Stage::LARVA || stage == Stage::PUPA) return;
    if (!bug.isWoodPlaced()) return;

    uint8_t style = GameEngine::ins().getWoodStyle();
    if (style >= WoodAssets::SPRITE_COUNT) style = 0;
    uint16_t offset = pgm_read_word(&WoodAssets::SPRITE_FRAMES[style].offset);
    uint16_t length = pgm_read_word(&WoodAssets::SPRITE_FRAMES[style].length);
    PixelRenderer::drawRgb565Rle(197 - WoodAssets::FRAME_W,
                                 GROUND_Y - WoodAssets::FRAME_H + 6,
                                 WoodAssets::FRAME_W,
                                 WoodAssets::FRAME_H,
                                 WoodAssets::SPRITE_RLE,
                                 offset, length);
}

