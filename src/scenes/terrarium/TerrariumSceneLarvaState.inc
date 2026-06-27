void TerrariumScene::enterLarvaState(LarvaState nextState, uint32_t nowMs) {
    if (larvaState == nextState) return;
    larvaState = nextState;
    larvaStateStartMs = nowMs;
    switch (nextState) {
        case LarvaState::EAT:
            larvaStateDurationMs = random(LARVA_EAT_MIN_MS, LARVA_EAT_MAX_MS + 1);
            break;
        case LarvaState::SLEEP:
            larvaStateDurationMs = random(LARVA_SLEEP_MIN_MS, LARVA_SLEEP_MAX_MS + 1);
            break;
        case LarvaState::IDLE:
        default:
            larvaStateDurationMs = random(LARVA_IDLE_MIN_MS, LARVA_IDLE_MAX_MS + 1);
            break;
    }
}

uint32_t TerrariumScene::larvaWalkStartDelayMs(uint8_t hunger) const {
    if (hunger <= 25) return 0;
    if (hunger <= 50) return 150;
    if (hunger <= 75) return 350;
    if (hunger <= LARVA_SUBSTRATE_EAT_HUNGER_THRESHOLD) return 600;
    return LARVA_WALK_START_MS;
}

uint32_t TerrariumScene::larvaSubstrateBiteIntervalMs(uint8_t hunger) const {
    if (hunger <= 25) return 500;
    if (hunger <= 50) return 650;
    if (hunger <= 75) return 780;
    return LARVA_SUBSTRATE_BITE_INTERVAL_MS;
}

void TerrariumScene::updateLarvaState(Bug& bug, uint32_t nowMs) {
    bug.setSleeping(false);
    uint8_t ageIndex = bug.getLarvaAgeIndex(GameEngine::ins().getGameNow());

    uint64_t lastEatGameMs = bug.getLastEatTime();
    bool ateSinceObserved = lastEatGameMs != 0 && lastEatGameMs != observedLarvaEatGameMs;
    bool wantsVisibleSubstrate = bug.getHunger() <= LARVA_SUBSTRATE_EAT_HUNGER_THRESHOLD &&
                                 hasLarvaSubstrate();
    uint32_t walkStartDelayMs = larvaWalkStartDelayMs(bug.getHunger());
    bool urgentFeeding = wantsVisibleSubstrate && bug.getHunger() <= LARVA_FEED_URGENT_HUNGER;
    int foodTargetX = bugX;
    bool foodTargetFaceRight = larvaFaceRight;
    bool hasFoodTarget = wantsVisibleSubstrate &&
                         findLarvaSubstrateFoodTarget(foodTargetX, &foodTargetFaceRight);
    if (hasFoodTarget && abs(foodTargetX - bugX) <= 2) {
        larvaFaceRight = foodTargetFaceRight;
        larvaTargetFaceRight = foodTargetFaceRight;
    }
    if (wantsVisibleSubstrate && consumeLarvaSubstrateBite(bug, nowMs)) {
        return;
    }
    if (ateSinceObserved) {
        observedLarvaEatGameMs = lastEatGameMs;
        larvaTargetX = bugX;
        if (larvaState == LarvaState::EAT) {
            uint32_t elapsed = nowMs - larvaStateStartMs;
            uint32_t minDuration = elapsed + random(LARVA_EAT_EMPTY_EXIT_MS, LARVA_EAT_EMPTY_EXIT_MS + 1800);
            if (larvaStateDurationMs < minDuration) larvaStateDurationMs = minDuration;
        } else {
            enterLarvaState(LarvaState::EAT, nowMs);
        }
        return;
    }

    if (larvaState == LarvaState::EAT) {
        uint32_t eatElapsedMs = nowMs - larvaStateStartMs;
        bool canKeepBitingSubstrate = wantsVisibleSubstrate && larvaNearSubstrateFood();
        bool shouldLeaveEmptyFood = !canKeepBitingSubstrate &&
                                    eatElapsedMs >= LARVA_EAT_EMPTY_EXIT_MS;
        if (shouldLeaveEmptyFood || eatElapsedMs >= larvaStateDurationMs) {
            enterLarvaState(LarvaState::IDLE, nowMs);
            larvaTargetX = bugX;
            larvaNextStepMs = wantsVisibleSubstrate ? nowMs : nowMs + walkStartDelayMs;
        }
        return;
    }

    if (larvaState == LarvaState::SLEEP) {
        if (ageIndex < 2 ||
            urgentFeeding ||
            bug.getHunger() < 35 ||
            nowMs - larvaStateStartMs >= larvaStateDurationMs) {
            enterLarvaState(LarvaState::IDLE, nowMs);
            larvaTargetX = bugX;
            larvaNextStepMs = urgentFeeding ? nowMs : nowMs + walkStartDelayMs;
        } else {
            bug.setSleeping(true);
        }
        return;
    }
    if (nowMs - larvaStateStartMs >= larvaStateDurationMs) {
        if (ageIndex == 2 && bug.getHunger() >= LARVA_SLEEP_SAFE_HUNGER && random(100) < 35) {
            enterLarvaState(LarvaState::SLEEP, nowMs);
            larvaTargetX = bugX;
            bug.setSleeping(true);
            return;
        }
        larvaState = LarvaState::IDLE;
        larvaStateStartMs = nowMs;
        larvaStateDurationMs = random(LARVA_IDLE_MIN_MS, LARVA_IDLE_MAX_MS + 1);
        larvaTargetX = bugX;
        larvaNextStepMs = nowMs + walkStartDelayMs;
        return;
    }

    if (nowMs - larvaStateStartMs < walkStartDelayMs) {
        return;
    }

    bool walkToSubstrateFood = hasFoodTarget && abs(foodTargetX - bugX) > 2;
    if (larvaTargetX < MIN_X || larvaTargetX > MAX_X || abs(larvaTargetX - bugX) <= 1) {
        int nextTarget = foodTargetX;
        if (!walkToSubstrateFood) {
            nextTarget = random(MIN_X, MAX_X + 1);
            if (abs(nextTarget - bugX) < LARVA_WALK_MIN_DELTA) {
                nextTarget = bugX < (MIN_X + MAX_X) / 2
                                 ? random(bugX + LARVA_WALK_MIN_DELTA, MAX_X + 1)
                                 : random(MIN_X, bugX - LARVA_WALK_MIN_DELTA + 1);
            }
        }
        larvaTargetX = nextTarget;
        larvaTargetFaceRight = walkToSubstrateFood ? foodTargetFaceRight : (larvaTargetX >= bugX);
        larvaFaceRight = larvaTargetFaceRight;
    }

    if (nowMs >= larvaNextStepMs) {
        int dx = larvaTargetX > bugX ? 1 : -1;
        int nextX = clampLarvaStepForSubstrate(bugX, bugX + dx);
        if (nextX == bugX && larvaCollidesWithSubstrateAt(bugX + dx)) {
            larvaTargetX = bugX;
        }
        bugX = nextX;
        if (bugX < MIN_X) bugX = MIN_X;
        if (bugX > MAX_X) bugX = MAX_X;
        larvaFaceRight = abs(larvaTargetX - bugX) <= 1 ? larvaTargetFaceRight : (dx > 0);
        larvaNextStepMs = nowMs + LARVA_WALK_STEP_MS;
    }
}
