void TerrariumScene::resetToy() {
    toyType = toyTypeForStyle(GameEngine::ins().getToyStyle());
    const ToySpec& spec = currentToySpec();
    toyVisible = isToyEnabled();
    toyEntryActive = false;
    toyEntryInteraction = toyButtonInteractionForStyle(GameEngine::ins().getToyStyle());
    toyEntryStartMs = 0;
    toyX = 172.0f;
    toyY = (float)(TOY_TANK_BOTTOM - spec.radius);
    toyVx = 0.0f;
    toyVy = 0.0f;
    toySpin = 0.0f;
    toyAngle = 0.0f;
    toyLastUpdateMs = Hal::ins().millis();
    toyLastHitMs = 0;
    toyAttackStartMs = 0;
    toyCharging = false;
    visitorToyCharging = false;
    toyChargeStartMs = 0;
    toyChargeDurationMs = 0;
    toyChargeDir = 1;
    visitorToyChargeStartMs = 0;
    visitorToyChargeDurationMs = 0;
    visitorToyChargeDir = 1;
    visitorToyAttackStartMs = 0;
    if (visitor.actor.state == AdultState::ATTACK_DOWN ||
        visitor.actor.state == AdultState::ATTACK_UP) {
        visitor.actor.state = AdultState::IDLE;
        visitor.actor.stateTimer = 0;
    }
    toyNoCatchUntilMs = 0;
    toyReplayReadyAfterImpact = false;
}

void TerrariumScene::clearToyAfterImpactReplay(uint32_t nowMs) {
    if (TOY_DEBUG_LOGS) {
        Serial.printf("[ToyThrow] clear busy-before visible=%u entry=%u localCharge=%u visitorCharge=%u x=%d y=%d vx=%d vy=%d noCatch=%ld\n",
                      toyVisible ? 1 : 0,
                      toyEntryActive ? 1 : 0,
                      toyCharging ? 1 : 0,
                      visitorToyCharging ? 1 : 0,
                      (int)roundf(toyX),
                      (int)roundf(toyY),
                      (int)roundf(toyVx),
                      (int)roundf(toyVy),
                      (long)(toyNoCatchUntilMs > nowMs ? toyNoCatchUntilMs - nowMs : 0));
    }
    toyVisible = false;
    toyEntryActive = false;
    toyCharging = false;
    visitorToyCharging = false;
    toyVx = 0.0f;
    toyVy = 0.0f;
    toySpin = 0.0f;
    toyLastUpdateMs = nowMs;
    toyLastHitMs = 0;
    toyAttackStartMs = 0;
    toyChargeStartMs = 0;
    toyChargeDurationMs = 0;
    visitorToyChargeStartMs = 0;
    visitorToyChargeDurationMs = 0;
    visitorToyAttackStartMs = 0;
    toyNoCatchUntilMs = 0;
    toyReplayReadyAfterImpact = false;
    if (visitor.actor.state == AdultState::ATTACK_DOWN ||
        visitor.actor.state == AdultState::ATTACK_UP) {
        visitor.actor.state = AdultState::IDLE;
        visitor.actor.stateTimer = 0;
    }
}

bool TerrariumScene::isToyEnabled() const {
    return toyButtonInteractionForStyle(GameEngine::ins().getToyStyle()) != ToyButtonInteraction::POKE;
}

TerrariumScene::ToyType TerrariumScene::toyTypeForStyle(uint8_t style) const {
    // 新增玩具时，在这里映射物理/绘制类型，并在下方映射 B 键交互方式。
    switch (style) {
        case GameEngine::TOY_BALL:
        default:
            return ToyType::SOCCER;
    }
}

TerrariumScene::ToyButtonInteraction
TerrariumScene::toyButtonInteractionForStyle(uint8_t style) const {
    switch (style) {
        case GameEngine::TOY_BALL:
            return ToyButtonInteraction::THROW_ARC;
        case GameEngine::TOY_NONE:
        default:
            return ToyButtonInteraction::POKE;
    }
}

uint32_t TerrariumScene::toyEntryDurationMs() const {
    switch (toyEntryInteraction) {
        case ToyButtonInteraction::DROP_DOWN:
            return TOY_DROP_MS;
        case ToyButtonInteraction::THROW_ARC:
        case ToyButtonInteraction::POKE:
        default:
            return TOY_THROW_MS;
    }
}

const TerrariumScene::ToySpec& TerrariumScene::currentToySpec() const {
    static const ToySpec SOCCER {
        ToyType::SOCCER,
        6,       // radius
        1.0f,    // mass
        520.0f,  // gravity
        0.76f,   // wallBounce
        0.50f,   // floorBounce
        0.992f,  // airDrag
        0.90f,   // rollFriction
        330.0f,  // baseImpulse
        190.0f,  // liftImpulse
    };

    switch (toyType) {
        case ToyType::SOCCER:
        default:
            return SOCCER;
    }
}

uint8_t TerrariumScene::toyInterestPercent(Temperament temperament) const {
    switch (temperament) {
        case Temperament::BRUTE:     return 90;
        case Temperament::SWIFT:     return 68;
        case Temperament::GIANT:     return 56;
        case Temperament::RESILIENT: return 45;
        case Temperament::BALANCED:  return 62;
        case Temperament::SPIRIT:    return 36;
    }
    return 50;
}

uint32_t TerrariumScene::toyChargeDurationFor(Temperament temperament) const {
    switch (temperament) {
        case Temperament::BRUTE:     return 420;
        case Temperament::SWIFT:     return 500;
        case Temperament::GIANT:     return 620;
        case Temperament::RESILIENT: return 700;
        case Temperament::BALANCED:  return 560;
        case Temperament::SPIRIT:    return 760;
    }
    return 600;
}

float TerrariumScene::toyStrengthPower(const Bug& bug) const {
    float cap = (float)bug.getStrCap();
    float ratio = cap <= 1.0f ? 0.0f : (bug.getStr() - 1.0f) / (cap - 1.0f);
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;

    float power = 0.55f + ratio * 0.75f;
    if (bug.getTemperament() == Temperament::BRUTE) power += 0.12f;
    if (bug.getTemperament() == Temperament::SPIRIT) power -= 0.08f;
    if (power < 0.42f) power = 0.42f;
    if (power > 1.35f) power = 1.35f;
    return power;
}

Temperament TerrariumScene::visitorTemperament() const {
    uint8_t t = visitor.temperament;
    if (t > (uint8_t)Temperament::SPIRIT) t = visitor.palette & 0x07;
    if (t > (uint8_t)Temperament::SPIRIT) t = (uint8_t)Temperament::BALANCED;
    return (Temperament)t;
}

float TerrariumScene::visitorToyStrengthPower() const {
    float cap = visitor.strCap < 1 ? 1.0f : (float)visitor.strCap;
    float ratio = cap <= 1.0f ? 0.0f : ((float)visitor.str - 1.0f) / (cap - 1.0f);
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;

    Temperament temperament = visitorTemperament();
    float power = 0.55f + ratio * 0.75f;
    if (temperament == Temperament::BRUTE) power += 0.12f;
    if (temperament == Temperament::SPIRIT) power -= 0.08f;
    if (power < 0.42f) power = 0.42f;
    if (power > 1.35f) power = 1.35f;
    return power;
}

void TerrariumScene::startToyCharge(uint32_t nowMs, int pushDir) {
    Bug& bug = GameEngine::ins().getBug();
    bool onWoodRoute = isAdultOnWoodRoute();
    if (!isAtInteractionDepth() || onWoodRoute) {
        if (TOY_DEBUG_LOGS) {
            Serial.printf("[WoodToy] charge-block push=%d x=%d z=%u state=%s onWood=%u toy=%d/%d vx=%d vy=%d noCatch=%ld\n",
                          pushDir,
                          bugX,
                          bugZ,
                          adultStateName(adultState),
                          onWoodRoute ? 1 : 0,
                          (int)roundf(toyX),
                          (int)roundf(toyY),
                          (int)roundf(toyVx),
                          (int)roundf(toyVy),
                          (long)(toyNoCatchUntilMs > nowMs ? toyNoCatchUntilMs - nowMs : 0));
        }
        deflectToyFromBeetle(pushDir);
        toyNoCatchUntilMs = nowMs + TOY_NO_CATCH_AFTER_REBOUND_MS;
        return;
    }
    if (pushDir == 0) pushDir = faceRight ? 1 : -1;
    if (random(100) >= toyInterestPercent(bug.getTemperament())) {
        if (TOY_DEBUG_LOGS) {
            Serial.printf("[WoodToy] charge-interest-miss push=%d x=%d state=%s toy=%d/%d vx=%d vy=%d\n",
                          pushDir,
                          bugX,
                          adultStateName(adultState),
                          (int)roundf(toyX),
                          (int)roundf(toyY),
                          (int)roundf(toyVx),
                          (int)roundf(toyVy));
        }
        deflectToyFromBeetle(pushDir);
        toyNoCatchUntilMs = nowMs + TOY_NO_CATCH_AFTER_REBOUND_MS;
        return;
    }

    const ToySpec& spec = currentToySpec();
    faceRight = pushDir > 0;
    toyCharging = true;
    toyChargeStartMs = nowMs;
    toyChargeDurationMs = toyChargeDurationFor(bug.getTemperament());
    toyChargeDir = pushDir;
    toyVx = 0.0f;
    toyVy = 0.0f;
    toySpin = 0.0f;
    toyX = bugX + pushDir * (34.0f * bug.getAdultScale() + spec.radius - 1.0f);
    toyY = (float)(TOY_TANK_BOTTOM - spec.radius);
    if (toyX < TOY_TANK_LEFT + spec.radius) toyX = TOY_TANK_LEFT + spec.radius;
    if (toyX > TOY_TANK_RIGHT - spec.radius) toyX = TOY_TANK_RIGHT - spec.radius;

    if (TOY_DEBUG_LOGS &&
        (bug.isWoodPlaced() || bugX >= WOOD_SURFACE_LEFT_X - WOOD_DISMOUNT_RAMP_PX)) {
        Serial.printf("[WoodToy] charge-start push=%d x=%d z=%u state=%s targetToy=%d/%d dur=%lu restResume=%ld alert=%ld\n",
                      pushDir,
                      bugX,
                      bugZ,
                      adultStateName(adultState),
                      (int)roundf(toyX),
                      (int)roundf(toyY),
                      (unsigned long)toyChargeDurationMs,
                      (long)(restResumeAllowedMs > nowMs ? restResumeAllowedMs - nowMs : 0),
                      (long)(alertUntilMs > nowMs ? alertUntilMs - nowMs : 0));
    }
    adultState = AdultState::IDLE;
    stateTimer = 0;
    stateDuration = (toyChargeDurationMs / 50) + 8;
    walkTargetIsRest = false;
    woodDismountActive = false;
}

void TerrariumScene::startVisitorToyCharge(uint32_t nowMs, int pushDir) {
    if (!visitor.active || visitor.falling) return;
    Temperament temperament = visitorTemperament();
    if (pushDir == 0) pushDir = visitor.faceRight ? 1 : -1;
    if (random(100) >= toyInterestPercent(temperament)) {
        deflectToyFromVisitor(nowMs, pushDir);
        toyNoCatchUntilMs = nowMs + TOY_NO_CATCH_AFTER_REBOUND_MS;
        return;
    }

    const ToySpec& spec = currentToySpec();
    visitor.faceRight = pushDir > 0;
    visitorToyCharging = true;
    visitorToyChargeStartMs = nowMs;
    visitorToyChargeDurationMs = toyChargeDurationFor(temperament);
    visitorToyChargeDir = pushDir;
    visitor.actor.state = AdultState::ATTACK_DOWN;
    visitor.actor.stateTimer = 0;
    visitor.actor.stateDuration = (visitorToyChargeDurationMs / 50) + 8;
    visitor.turning = false;
    visitor.nextWanderMs = 0;
    toyVx = 0.0f;
    toyVy = 0.0f;
    toySpin = 0.0f;
    toyX = visitor.x + pushDir * (34.0f * visitorAdultScale() + spec.radius - 1.0f);
    toyY = (float)(TOY_TANK_BOTTOM - spec.radius);
    if (toyX < TOY_TANK_LEFT + spec.radius) toyX = TOY_TANK_LEFT + spec.radius;
    if (toyX > TOY_TANK_RIGHT - spec.radius) toyX = TOY_TANK_RIGHT - spec.radius;
}

void TerrariumScene::triggerToyHit(uint32_t nowMs, int pushDir, float chargeRatio) {
    Bug& bug = GameEngine::ins().getBug();
    const ToySpec& spec = currentToySpec();
    if (pushDir == 0) pushDir = faceRight ? 1 : -1;
    if (chargeRatio < 0.0f) chargeRatio = 0.0f;
    if (chargeRatio > 1.0f) chargeRatio = 1.0f;

    float power = toyStrengthPower(bug);
    float chargeBonus = 0.72f + chargeRatio * 0.70f;
    float massScale = spec.mass <= 0.1f ? 1.0f : (1.0f / spec.mass);
    faceRight = pushDir > 0;
    toyVx = pushDir * spec.baseImpulse * power * chargeBonus * massScale;
    toyVy = -spec.liftImpulse * (0.72f + power * 0.35f) * chargeBonus * massScale;
    toySpin = pushDir * (7.0f + 10.0f * power + 4.0f * chargeRatio);
    toyLastHitMs = nowMs;
    toyAttackStartMs = nowMs;
    GameEngine::ins().recordVisitLocalPlay();
    toyNoCatchUntilMs = nowMs + TOY_NO_CATCH_AFTER_HIT_MS;
    toyCharging = false;
    toyChargeStartMs = 0;
    if (TOY_DEBUG_LOGS &&
        (bug.isWoodPlaced() || bugX >= WOOD_SURFACE_LEFT_X - WOOD_DISMOUNT_RAMP_PX)) {
        Serial.printf("[WoodToy] charge-hit push=%d ratio=%d x=%d state=%s toyV=%d/%d noCatch=%lu\n",
                      pushDir,
                      (int)roundf(chargeRatio * 100.0f),
                      bugX,
                      adultStateName(adultState),
                      (int)roundf(toyVx),
                      (int)roundf(toyVy),
                      (unsigned long)TOY_NO_CATCH_AFTER_HIT_MS);
    }
}

void TerrariumScene::reactLocalToToyImpact(uint32_t nowMs, int pushDir) {
    Bug& bug = GameEngine::ins().getBug();
    if (bug.getStage() != Stage::ADULT || bug.isDead()) return;
    if (!isAtInteractionDepth()) return;
    if (adultState == AdultState::EAT) return;

    bool wasOnWood = isAdultOnWoodRoute();
    if (TOY_DEBUG_LOGS) {
        Serial.printf("[ToyThrow] impact local push=%d state=%s onWood=%u x=%d foot=%d\n",
                      pushDir,
                      adultStateName(adultState),
                      wasOnWood ? 1 : 0,
                      bugX,
                      adultFootY());
    }
    if (adultState == AdultState::REST || wasOnWood) {
        restResumeAllowedMs = nowMs + random(REST_WAKEUP_COOLDOWN_MIN_MS,
                                             REST_WAKEUP_COOLDOWN_MAX_MS + 1);
    }
    faceRight = pushDir >= 0;
    turnTargetFaceRight = faceRight;
    pokeReactionStartMs = nowMs;
    pokeReactionWasPoked = true;
    pokeThreatenEndMs = nowMs + POKE_REACTION_MS;
    alertUntilMs = nowMs + random(ALERT_MIN_MS, ALERT_MAX_MS + 1);
    GameEngine::ins().recordVisitLocalPlay();
    adultState = AdultState::IDLE;
    woodDismountActive = false;
    stateTimer = 0;
    setIdleDuration();
}

void TerrariumScene::reactVisitorToToyImpact(uint32_t nowMs, int pushDir) {
    if (!visitor.active || visitor.falling) return;
    if (visitor.actor.state == AdultState::REST || visitor.actor.state == AdultState::EAT) return;
    if (visitor.actor.state == AdultState::ATTACK_DOWN ||
        visitor.actor.state == AdultState::ATTACK_UP) {
        return;
    }

    if (TOY_DEBUG_LOGS) {
        Serial.printf("[ToyThrow] impact visitor push=%d state=%s x=%d targetY=%d\n",
                      pushDir,
                      adultStateName(visitor.actor.state),
                      visitor.x,
                      visitor.targetY);
    }
    visitorToyCharging = false;
    visitorToyChargeStartMs = 0;
    visitor.faceRight = pushDir >= 0;
    beginActorWalkTo(visitor.actor, visitor.x);
    visitor.actor.state = AdultState::THREATEN;
    visitor.actor.stateTimer = 0;
    visitor.actor.stateDuration = POKE_REACTION_MS;
    visitor.actor.threatenStartMs = nowMs;
    visitor.actor.threatenEndMs = nowMs + POKE_REACTION_MS;
    GameEngine::ins().recordVisitRemotePlay();
    visitorIntentUntilMs = nowMs + VISITOR_INTENT_MS;
    visitor.turning = false;
    visitor.nextWanderMs = visitor.actor.threatenEndMs;
    logVisitorState("toy-impact-threaten", nowMs);
}

void TerrariumScene::reboundToyFromEntryImpact(uint32_t nowMs, int pushDir,
                                               float beetleTop, const ToySpec& spec) {
    if (pushDir == 0) pushDir = 1;
    toyReplayReadyAfterImpact = true;
    toyY = beetleTop - (float)spec.radius - 1.0f;
    if (toyY < TOY_TANK_TOP + spec.radius) toyY = (float)(TOY_TANK_TOP + spec.radius);
    if (toyX < TOY_TANK_LEFT + spec.radius) toyX = (float)(TOY_TANK_LEFT + spec.radius);
    if (toyX > TOY_TANK_RIGHT - spec.radius) toyX = (float)(TOY_TANK_RIGHT - spec.radius);

    float vx = fabsf(toyVx) * 0.48f;
    float minVx = spec.baseImpulse * 0.34f;
    if (vx < minVx) vx = minVx;
    toyVx = vx * 0.9f * (float)pushDir;
    float vy = fabsf(toyVy) * 0.68f;
    float minVy = spec.liftImpulse * 0.92f;
    if (vy < minVy) vy = minVy;
    toyVy = -vy * 0.9f;
    toySpin = (float)(pushDir * 11) * 0.9f;
    toyNoCatchUntilMs = nowMs + TOY_NO_CATCH_AFTER_REBOUND_MS;
    if (TOY_DEBUG_LOGS) {
        Serial.printf("[ToyThrow] rebound entry push=%d replay=%u x=%d y=%d vx=%d vy=%d noCatch=%lu\n",
                      pushDir,
                      toyReplayReadyAfterImpact ? 1 : 0,
                      (int)roundf(toyX),
                      (int)roundf(toyY),
                      (int)roundf(toyVx),
                      (int)roundf(toyVy),
                      (unsigned long)TOY_NO_CATCH_AFTER_REBOUND_MS);
    }
}

void TerrariumScene::triggerVisitorToyHit(uint32_t nowMs, int pushDir, float chargeRatio) {
    const ToySpec& spec = currentToySpec();
    if (pushDir == 0) pushDir = visitor.faceRight ? 1 : -1;
    if (chargeRatio < 0.0f) chargeRatio = 0.0f;
    if (chargeRatio > 1.0f) chargeRatio = 1.0f;

    float power = visitorToyStrengthPower();
    float chargeBonus = 0.72f + chargeRatio * 0.70f;
    float massScale = spec.mass <= 0.1f ? 1.0f : (1.0f / spec.mass);
    visitor.faceRight = pushDir > 0;
    toyVx = pushDir * spec.baseImpulse * power * chargeBonus * massScale;
    toyVy = -spec.liftImpulse * (0.72f + power * 0.35f) * chargeBonus * massScale;
    toySpin = pushDir * (7.0f + 10.0f * power + 4.0f * chargeRatio);
    toyLastHitMs = nowMs;
    toyNoCatchUntilMs = nowMs + TOY_NO_CATCH_AFTER_HIT_MS;
    visitorToyCharging = false;
    visitorToyChargeStartMs = 0;
    visitorToyAttackStartMs = nowMs;
    GameEngine::ins().recordVisitRemotePlay();
    visitor.actor.state = AdultState::ATTACK_UP;
    visitor.actor.stateTimer = 0;
    visitor.actor.stateDuration = (TOY_ATTACK_UP_MS / 50) + 4;
}
