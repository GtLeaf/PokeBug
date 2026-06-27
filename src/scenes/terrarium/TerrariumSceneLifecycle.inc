void TerrariumScene::onEnter() {
    animFrame = 0;
    invalidateRenderCaches();
    resetPressStart = 0;
    resetting = false;
    visitRecallConfirm = false;
    visitPingInFlight = false;
    visitPingFailures = 0;
    lastVisitPingMs = Hal::ins().millis();
    lastVisitStatusMs = Hal::ins().millis();
    lastVisitStatusRxMs = Hal::ins().millis();
    visitorIntentUntilMs = 0;
    visitorEatRequested = false;
    lastVisitEatIntentMs = 0;
    visitEatRetryAfterMs = 0;
    nextVisitPlayIntentMs = Hal::ins().millis() + random(VISIT_PLAY_INTENT_MIN_MS,
                                                         VISIT_PLAY_INTENT_MAX_MS + 1);
    pendingVisitEatResult = false;
    visitorStateLogInitialized = false;
    lastVisitorStateLogMs = 0;
    resetToy();

    Bug& bug = GameEngine::ins().getBug();
    Stage stage = bug.getStage();
    if (stage == Stage::EGG || stage == Stage::LARVA || stage == Stage::PUPA) {
        bug.setFoodTray(0, (FoodType)GameEngine::ins().getFoodStyle());
        bug.removeWood();
    }
    if (stage != Stage::EGG && stage != Stage::LARVA) {
        clearLarvaSubstrate();
    } else if (substrateLastUpdateMs == 0) {
        substrateLastUpdateMs = Hal::ins().millis();
    }

    const TerrariumViewState& saved = GameEngine::ins().getTerrariumViewState();
    if (saved.valid && (bug.getStage() == Stage::EGG || bug.getStage() == Stage::LARVA)) {
        restoreLarvaSubstrateFromViewState(saved);
    }
    if (saved.valid && bug.getStage() == Stage::LARVA && !bug.isDead()) {
        bugX = saved.bugX;
        if (bugX < MIN_X) bugX = MIN_X;
        if (bugX > MAX_X) bugX = MAX_X;
        bugY = saved.bugY;
        bugZ = 0;
        animFrame = saved.animFrame;
        faceRight = saved.faceRight;
        larvaFaceRight = faceRight;
        larvaTargetFaceRight = faceRight;
        larvaTargetX = bugX;
        larvaState = LarvaState::IDLE;
        larvaStateStartMs = Hal::ins().millis();
        larvaStateDurationMs = random(LARVA_IDLE_MIN_MS, LARVA_IDLE_MAX_MS + 1);
        larvaNextStepMs = larvaStateStartMs + LARVA_WALK_START_MS;
        observedLarvaEatGameMs = bug.getLastEatTime();
        startPendingVisitIfAny();
        return;
    }
    if (saved.valid && isMobileBeetleStage(bug.getStage()) && !bug.isDead()) {
        bugX = saved.bugX;
        bugY = saved.bugY;
        bugZ = clampDepthZ(saved.bugZ);
        animFrame = saved.animFrame;
        adultState = saved.adultState <= (uint8_t)AdultState::ATTACK_UP ?
                     (AdultState)saved.adultState : AdultState::IDLE;
        faceRight = saved.faceRight;
        turnTargetFaceRight = saved.turnTargetFaceRight;
        walkAfterTurn = saved.walkAfterTurn;
        slideAfterTurn = saved.slideAfterTurn;
        climbAfterTurn = saved.climbAfterTurn;
        eatAfterTurn = false;
        walkTargetIsRest = false;
        woodDismountActive = false;
        turnFrameIndex = saved.turnFrameIndex;
        targetX = saved.targetX;
        targetZ = clampDepthZ(saved.targetZ);
        slideTargetX = saved.slideTargetX;
        climbTargetX = saved.climbTargetX;
        tiltHighSideIsRight = saved.tiltHighSideIsRight;
        stateTimer = saved.stateTimer;
        stateDuration = saved.stateDuration;
        eatFrameInterval = saved.eatFrameInterval;
        eatBitesThisSession = saved.eatBitesThisSession;
        restResumeAllowedMs = saved.restResumeAllowedMs;
        foodRefillGraceUntilMs = saved.foodRefillGraceUntilMs;
        alertUntilMs = saved.alertUntilMs;
        mind.resetActivityTimer(Hal::ins().millis());
        restoreVisitorFromViewState(saved);
        startPendingVisitIfAny();
        return;
    }

    resetLocalViewState();
    startPendingVisitIfAny();
}

void TerrariumScene::onExit() {
    persistViewState();
    GameEngine::ins().getBug().setSleeping(false);
}

void TerrariumScene::persistViewState() {
    Bug& bug = GameEngine::ins().getBug();
    if ((isMobileBeetleStage(bug.getStage()) ||
         bug.getStage() == Stage::EGG ||
         bug.getStage() == Stage::LARVA) &&
        !bug.isDead()) {
        TerrariumViewState state;
        state.bugX = bugX;
        state.bugY = bugY;
        state.bugZ = bug.getStage() == Stage::ADULT ? bugZ : 0;
        state.animFrame = animFrame;
        state.adultState = (uint8_t)adultState;
        state.faceRight = bug.getStage() == Stage::LARVA ? larvaFaceRight : faceRight;
        state.turnTargetFaceRight = turnTargetFaceRight;
        state.walkAfterTurn = walkAfterTurn;
        state.slideAfterTurn = slideAfterTurn;
        state.climbAfterTurn = climbAfterTurn;
        // walkTargetIsRest 是短暂运行时意图，不持久化，恢复后按普通 WALK 继续。
        state.turnFrameIndex = turnFrameIndex;
        state.targetX = targetX;
        state.targetZ = bug.getStage() == Stage::ADULT ? targetZ : 0;
        state.slideTargetX = slideTargetX;
        state.climbTargetX = climbTargetX;
        state.tiltHighSideIsRight = tiltHighSideIsRight;
        state.stateTimer = stateTimer;
        state.stateDuration = stateDuration;
        state.eatFrameInterval = eatFrameInterval;
        state.eatBitesThisSession = eatBitesThisSession;
        state.restResumeAllowedMs = restResumeAllowedMs;
        state.foodRefillGraceUntilMs = foodRefillGraceUntilMs;
        state.alertUntilMs = alertUntilMs;
        if (bug.getStage() == Stage::EGG || bug.getStage() == Stage::LARVA) {
            for (uint8_t i = 0; i < SUBSTRATE_SLOT_COUNT; ++i) {
                state.substrateLevels[i] = substrateLevels[i];
                state.substrateBites[i] = substrateBites[i];
            }
            for (uint8_t i = 0; i < SUBSTRATE_DROP_SLOTS; ++i) {
                if (!substrateDrops[i].active) continue;
                state.substrateDropActiveMask |= (uint8_t)(1 << i);
                state.substrateDropSlots[i] = substrateDrops[i].slot;
                state.substrateDropLevels[i] = substrateDrops[i].level;
                state.substrateDropY[i] = (int16_t)roundf(substrateDrops[i].y);
                state.substrateDropVy[i] = (int16_t)roundf(substrateDrops[i].vy);
            }
        }

        uint32_t nowMs = Hal::ins().millis();
        if (isMobileBeetleStage(bug.getStage()) &&
            GameEngine::ins().isVisitHost() && visitor.active && nowMs < visitor.untilMs) {
            state.visitorActive = true;
            state.visitorFalling = visitor.falling;
            state.visitorX = visitor.x;
            state.visitorY = visitor.y;
            state.visitorFromY = visitor.fromY;
            state.visitorTargetY = visitor.targetY;
            state.visitorSiz = visitor.siz;
            state.visitorPalette = visitor.palette;
            state.visitorFaceRight = visitor.faceRight;
            state.visitorRemainingMs = visitor.untilMs - nowMs;
            state.visitorDropElapsedMs = visitor.falling ? nowMs - visitor.startMs : VISITOR_DROP_MS;
            if (state.visitorDropElapsedMs > VISITOR_DROP_MS) {
                state.visitorDropElapsedMs = VISITOR_DROP_MS;
            }
        }
        GameEngine::ins().saveTerrariumViewState(state);
    } else {
        GameEngine::ins().clearTerrariumViewState();
    }
}

void TerrariumScene::resetLocalViewState() {
    bugX = 120;
    bugY = 80;
    bugZ = 0;
    animFrame = 0;
    adultState = AdultState::IDLE;
    faceRight = true;
    turnTargetFaceRight = true;
    walkAfterTurn = false;
    slideAfterTurn = false;
    climbAfterTurn = false;
    eatAfterTurn = false;
    walkTargetIsRest = false;
    woodDismountActive = false;
    turnFrameIndex = 0;
    targetX = bugX;
    targetZ = 0;
    slideTargetX = bugX;
    climbTargetX = bugX;
    tiltHighSideIsRight = true;
    eatFrameInterval = 0;
    eatBitesThisSession = 0;
    eatLastBiteMs = 0;
    larvaState = LarvaState::IDLE;
    larvaStateStartMs = Hal::ins().millis();
    larvaStateDurationMs = random(LARVA_IDLE_MIN_MS, LARVA_IDLE_MAX_MS + 1);
    larvaTargetX = bugX;
    larvaNextStepMs = larvaStateStartMs;
    larvaFaceRight = faceRight;
    larvaTargetFaceRight = faceRight;
    observedLarvaEatGameMs = GameEngine::ins().getBug().getLastEatTime();
    substrateLastUpdateMs = Hal::ins().millis();
    restResumeAllowedMs = 0;
    foodRefillGraceUntilMs = 0;
    alertUntilMs = 0;
    resetToy();
    mind.resetActivityTimer(Hal::ins().millis());
    stateTimer = 0;
    setIdleDuration();
}

void TerrariumScene::restoreLarvaSubstrateFromViewState(const TerrariumViewState& saved) {
    memset(substrateLevels, 0, sizeof(substrateLevels));
    memset(substrateBites, 0, sizeof(substrateBites));
    for (uint8_t i = 0; i < SUBSTRATE_DROP_SLOTS; ++i) {
        substrateDrops[i].active = false;
    }

    uint8_t maxBites = substrateBiteCapacity(3);
    for (uint8_t i = 0; i < SUBSTRATE_SLOT_COUNT; ++i) {
        uint8_t bites = saved.substrateBites[i];
        uint8_t level = saved.substrateLevels[i];
        if (level == 0 || bites == 0) continue;
        if (bites > maxBites) bites = maxBites;
        setSubstrateBites(i, bites);
    }

    for (uint8_t i = 0; i < SUBSTRATE_DROP_SLOTS; ++i) {
        if ((saved.substrateDropActiveMask & (1 << i)) == 0) continue;
        uint8_t slot = saved.substrateDropSlots[i];
        if (slot >= SUBSTRATE_SLOT_COUNT) continue;

        uint8_t level = saved.substrateDropLevels[i];
        if (level == 0) level = 1;
        if (level > 3) level = 3;

        SubstrateDrop& drop = substrateDrops[i];
        drop.active = true;
        drop.slot = slot;
        drop.level = level;
        drop.y = (float)saved.substrateDropY[i];
        drop.vy = (float)saved.substrateDropVy[i];
    }

    substrateFadeActive = false;
    substrateFadeStartMs = 0;
    substrateLastUpdateMs = Hal::ins().millis();
}

void TerrariumScene::restoreVisitorFromViewState(const TerrariumViewState& saved) {
    visitor.active = false;
    if (!saved.visitorActive || saved.visitorRemainingMs == 0) return;
    if (!GameEngine::ins().isVisitHost()) return;

    uint32_t nowMs = Hal::ins().millis();
    visitor.active = true;
    visitor.falling = saved.visitorFalling;
    visitor.x = saved.visitorX;
    visitor.y = saved.visitorY;
    visitor.fromY = saved.visitorFromY;
    visitor.targetY = saved.visitorTargetY;
    visitor.siz = saved.visitorSiz;
    visitor.palette = saved.visitorPalette;
    visitor.str = 1;
    visitor.strCap = 6;
    visitor.temperament = visitor.palette & 0x07;
    visitor.faceRight = saved.visitorFaceRight;
    visitor.untilMs = nowMs + saved.visitorRemainingMs;
    beginActorWalkTo(visitor.actor, visitor.x);
    visitor.actor.state = AdultState::IDLE;
    visitor.nextStepMs = nowMs + VISITOR_STEP_MS;
    visitor.nextWanderMs = nowMs + random(VISITOR_IDLE_MIN_MS, VISITOR_IDLE_MAX_MS + 1);
    visitor.turning = false;

    if (visitor.falling) {
        uint32_t dropElapsed = saved.visitorDropElapsedMs;
        if (dropElapsed >= VISITOR_DROP_MS) {
            visitor.falling = false;
            visitor.y = visitor.targetY;
            visitor.startMs = nowMs >= VISITOR_DROP_MS ? nowMs - VISITOR_DROP_MS : 0;
        } else {
            visitor.startMs = nowMs >= dropElapsed ? nowMs - dropElapsed : 0;
        }
    } else {
        visitor.y = visitor.targetY;
        visitor.startMs = nowMs >= VISITOR_DROP_MS ? nowMs - VISITOR_DROP_MS : 0;
    }
    logVisitorState("restore", nowMs);
}

void TerrariumScene::startPendingVisitIfAny() {
    GameEngine& engine = GameEngine::ins();
    if (engine.hasActiveVisitSession()) {
        uint32_t remainingMs = engine.getVisitRemainingMs();
        if (remainingMs == 0) {
            visitor.active = false;
            return;
        }

        if (!engine.isVisitHost()) {
            visitor.active = false;
            return;
        }

        const VisitBugSnapshot& remote = engine.getVisitRemoteBug();
        if (visitor.active) {
            visitor.untilMs = Hal::ins().millis() + remainingMs;
            return;
        }

        Bug& bug = engine.getBug();
        if (!isMobileBeetleStage(bug.getStage()) || bug.isDead()) {
            visitor.active = false;
            return;
        }

        uint32_t nowMs = Hal::ins().millis();
        visitor.active = true;
        visitor.falling = true;
        visitor.siz = remote.siz;
        visitor.palette = remote.palette;
        visitor.str = remote.str;
        visitor.strCap = remote.strCap;
        visitor.temperament = remote.temperament;
        visitor.x = bugX < 120 ? 170 : 70;
        visitor.fromY = TOY_TANK_TOP - 34;
        visitor.targetY = GROUND_Y;
        visitor.y = visitor.fromY;
        visitor.faceRight = visitor.x < bugX;
        visitor.startMs = nowMs;
        visitor.untilMs = nowMs + remainingMs;
        beginActorWalkTo(visitor.actor, visitor.x);
        visitor.actor.state = AdultState::IDLE;
        visitor.nextStepMs = nowMs + VISITOR_STEP_MS;
        visitor.nextWanderMs = 0;
        visitor.turning = false;
        chooseVisitorTarget();
        logVisitorState("spawn-session", nowMs);
        return;
    }

    if (!GameEngine::ins().hasPendingVisitBug()) {
        visitor.active = false;
        return;
    }

    VisitBugSnapshot pending = GameEngine::ins().takePendingVisitBug();
    Bug& bug = GameEngine::ins().getBug();
    if (!isMobileBeetleStage(bug.getStage()) || bug.isDead()) {
        visitor.active = false;
        return;
    }

    uint32_t nowMs = Hal::ins().millis();
    visitor.active = true;
    visitor.falling = true;
    visitor.siz = pending.siz;
    visitor.palette = pending.palette;
    visitor.str = pending.str;
    visitor.strCap = pending.strCap;
    visitor.temperament = pending.temperament;
    visitor.x = bugX < 120 ? 170 : 70;
    visitor.fromY = TOY_TANK_TOP - 34;
    visitor.targetY = GROUND_Y;
    visitor.y = visitor.fromY;
    visitor.faceRight = visitor.x < bugX;
    visitor.startMs = nowMs;
    visitor.untilMs = nowMs + GameEngine::VISIT_MAX_MS;
    beginActorWalkTo(visitor.actor, visitor.x);
    visitor.actor.state = AdultState::IDLE;
    visitor.nextStepMs = nowMs + VISITOR_STEP_MS;
    visitor.nextWanderMs = 0;
    visitor.turning = false;
    chooseVisitorTarget();
    logVisitorState("spawn-pending", nowMs);
}
