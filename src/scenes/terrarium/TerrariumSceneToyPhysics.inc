void TerrariumScene::updateToyPhysics(uint32_t nowMs) {
    if (!isToyEnabled()) {
        toyVisible = false;
        toyEntryActive = false;
        toyCharging = false;
        visitorToyCharging = false;
        visitorToyAttackStartMs = 0;
        if (visitor.actor.state == AdultState::ATTACK_DOWN ||
            visitor.actor.state == AdultState::ATTACK_UP) {
            visitor.actor.state = AdultState::IDLE;
            visitor.actor.stateTimer = 0;
        }
        return;
    }

    if (toyEntryActive) {
        if (nowMs - toyEntryStartMs >= toyEntryDurationMs()) {
            finishToyEntry(nowMs);
        }
        toyLastUpdateMs = nowMs;
        return;
    }

    if (!toyVisible) return;

    const ToySpec& spec = currentToySpec();

    if (toyCharging) {
        Bug& bug = GameEngine::ins().getBug();
        toyChargeDir = toyChargeDir >= 0 ? 1 : -1;
        faceRight = toyChargeDir > 0;
        toyX = bugX + toyChargeDir * (34.0f * bug.getAdultScale() + spec.radius - 1.0f);
        toyY = (float)(TOY_TANK_BOTTOM - spec.radius);
        if (toyX < TOY_TANK_LEFT + spec.radius) toyX = TOY_TANK_LEFT + spec.radius;
        if (toyX > TOY_TANK_RIGHT - spec.radius) toyX = TOY_TANK_RIGHT - spec.radius;
        uint32_t elapsed = nowMs - toyChargeStartMs;
        if (elapsed >= toyChargeDurationMs) {
            triggerToyHit(nowMs, toyChargeDir, 1.0f);
        }
        toyLastUpdateMs = nowMs;
        return;
    }

    if (visitorToyCharging) {
        visitorToyChargeDir = visitorToyChargeDir >= 0 ? 1 : -1;
        visitor.faceRight = visitorToyChargeDir > 0;
        toyX = visitor.x + visitorToyChargeDir *
               (34.0f * visitorAdultScale() + spec.radius - 1.0f);
        toyY = (float)(TOY_TANK_BOTTOM - spec.radius);
        if (toyX < TOY_TANK_LEFT + spec.radius) toyX = TOY_TANK_LEFT + spec.radius;
        if (toyX > TOY_TANK_RIGHT - spec.radius) toyX = TOY_TANK_RIGHT - spec.radius;
        uint32_t elapsed = nowMs - visitorToyChargeStartMs;
        if (elapsed >= visitorToyChargeDurationMs) {
            triggerVisitorToyHit(nowMs, visitorToyChargeDir, 1.0f);
        }
        toyLastUpdateMs = nowMs;
        return;
    }

    uint32_t elapsedMs = toyLastUpdateMs == 0 ? 33 : nowMs - toyLastUpdateMs;
    toyLastUpdateMs = nowMs;
    if (elapsedMs > 80) elapsedMs = 80;
    float dt = elapsedMs / 1000.0f;

    float ax = 0.0f;
    float ay = 0.0f;
    float az = 0.0f;
    Hal::ins().getAccel(ax, ay, az);
    if (fabsf(ax) > 0.08f) {
        toyVx += -ax * TOY_TILT_ACCEL * dt;
    }

    toyVy += spec.gravity * dt;
    toyX += toyVx * dt;
    toyY += toyVy * dt;
    toyAngle += toySpin * dt;
    toyVx *= spec.airDrag;

    float left = (float)(TOY_TANK_LEFT + spec.radius);
    float right = (float)(TOY_TANK_RIGHT - spec.radius);
    float top = (float)(TOY_TANK_TOP + spec.radius);
    float bottom = (float)(TOY_TANK_BOTTOM - spec.radius);

    if (toyX < left) {
        toyX = left;
        toyVx = fabsf(toyVx) * spec.wallBounce;
        toySpin = fabsf(toySpin);
        toyNoCatchUntilMs = nowMs + TOY_NO_CATCH_AFTER_REBOUND_MS;
    } else if (toyX > right) {
        toyX = right;
        toyVx = -fabsf(toyVx) * spec.wallBounce;
        toySpin = -fabsf(toySpin);
        toyNoCatchUntilMs = nowMs + TOY_NO_CATCH_AFTER_REBOUND_MS;
    }
    if (toyY < top) {
        toyY = top;
        toyVy = fabsf(toyVy) * spec.wallBounce;
        toyNoCatchUntilMs = nowMs + TOY_NO_CATCH_AFTER_REBOUND_MS;
    } else if (toyY > bottom) {
        toyY = bottom;
        if (fabsf(toyVy) < 48.0f) {
            toyVy = 0.0f;
        } else {
            toyVy = -fabsf(toyVy) * spec.floorBounce;
            toyNoCatchUntilMs = nowMs + TOY_NO_CATCH_AFTER_REBOUND_MS;
        }
        toyVx *= spec.rollFriction;
        toySpin *= spec.rollFriction;
        if (fabsf(toyVx) < 3.0f) toyVx = 0.0f;
        if (fabsf(toySpin) < 0.4f) toySpin = 0.0f;
    }

    handleToyVisitorCollision(nowMs, spec, left, right);

    Bug& bug = GameEngine::ins().getBug();
    if (adultState == AdultState::REST || adultState == AdultState::EAT) return;

    float adultScale = bug.getAdultScale();
    float beetleHalfW = TOY_BEETLE_HIT_HALF_W * adultScale;
    float beetleTop = GROUND_Y - TOY_BEETLE_HIT_TOP * adultScale;
    float beetleBottom = GROUND_Y - TOY_BEETLE_HIT_BOTTOM;
    float beetleLeft = bugX - beetleHalfW;
    float beetleRight = bugX + beetleHalfW;

    if (circleHitsRect(toyX, toyY, (float)spec.radius,
                       beetleLeft, beetleTop, beetleRight, beetleBottom)) {
        int pushDir = toyX >= bugX ? 1 : -1;
        bool topImpact = toyVy > 0.0f &&
                         toyY <= beetleTop + (float)spec.radius + 4.0f &&
                         toyX >= beetleLeft - (float)spec.radius &&
                         toyX <= beetleRight + (float)spec.radius;
        if (topImpact) {
            if (fabsf(toyVx) > 8.0f) pushDir = toyVx > 0.0f ? 1 : -1;
            toyY = beetleTop - (float)spec.radius - 1.0f;
            if (toyY < top) toyY = top;
            toyVy = -fmaxf(fabsf(toyVy) * 0.72f, spec.liftImpulse * 0.42f);
            if (fabsf(toyVx) < 28.0f) toyVx = (float)pushDir * 28.0f;
            else toyVx *= 0.88f;
            toySpin = (float)pushDir * (fabsf(toySpin) + 5.0f);
            toyNoCatchUntilMs = nowMs + TOY_NO_CATCH_AFTER_REBOUND_MS;
            return;
        }
        toyX = pushDir > 0 ? beetleRight + spec.radius + 1.0f
                           : beetleLeft - spec.radius - 1.0f;
        if (toyX < left) toyX = left;
        if (toyX > right) toyX = right;
        float speed = sqrtf(toyVx * toyVx + toyVy * toyVy);
        bool toyMovingIntoBeetle = (pushDir > 0 && toyVx < -20.0f) ||
                                   (pushDir < 0 && toyVx > 20.0f);
        bool canReact = nowMs >= toyNoCatchUntilMs &&
                        nowMs - toyLastHitMs >= TOY_HIT_COOLDOWN_MS &&
                        speed <= TOY_CATCH_MAX_SPEED &&
                        !toyMovingIntoBeetle;
        if (canReact) {
            startToyCharge(nowMs, pushDir);
        } else {
            deflectToyFromBeetle(pushDir);
        }
    }
}

void TerrariumScene::deflectToyFromBeetle(int pushDir) {
    if (pushDir == 0) pushDir = toyX >= bugX ? 1 : -1;
    toyVx = fabsf(toyVx) * (float)pushDir * 0.45f;
    if (fabsf(toyVx) < 24.0f) toyVx = 24.0f * (float)pushDir;
    if (toyVy > 0.0f) toyVy *= 0.55f;
    toySpin *= -0.4f;
}

void TerrariumScene::deflectToyFromVisitor(uint32_t nowMs, int pushDir) {
    if (pushDir == 0) pushDir = toyX >= visitor.x ? 1 : -1;
    toyVx = fabsf(toyVx) * (float)pushDir * 0.52f;
    if (fabsf(toyVx) < 32.0f) toyVx = 32.0f * (float)pushDir;
    if (toyVy < 28.0f) toyVy = 28.0f;
    toySpin = -(float)pushDir * (fabsf(toySpin) + 3.0f);
    toyNoCatchUntilMs = nowMs + TOY_NO_CATCH_AFTER_REBOUND_MS;
}

void TerrariumScene::handleToyVisitorCollision(uint32_t nowMs, const ToySpec& spec,
                                               float left, float right) {
    if (!visitor.active || visitor.falling) return;
    if (visitor.actor.state == AdultState::REST || visitor.actor.state == AdultState::EAT) return;

    float scale = visitorAdultScale();
    float beetleHalfW = TOY_BEETLE_HIT_HALF_W * scale;
    float beetleTop = (float)visitor.targetY - TOY_BEETLE_HIT_TOP * scale;
    float beetleBottom = (float)visitor.targetY - TOY_BEETLE_HIT_BOTTOM;
    float beetleLeft = (float)visitor.x - beetleHalfW;
    float beetleRight = (float)visitor.x + beetleHalfW;

    if (!circleHitsRect(toyX, toyY, (float)spec.radius,
                        beetleLeft, beetleTop, beetleRight, beetleBottom)) {
        return;
    }

    int pushDir = toyX >= visitor.x ? 1 : -1;
    bool topImpact = toyVy > 0.0f &&
                     toyY <= beetleTop + (float)spec.radius + 4.0f &&
                     toyX >= beetleLeft - (float)spec.radius &&
                     toyX <= beetleRight + (float)spec.radius;
    if (topImpact) {
        if (fabsf(toyVx) > 8.0f) pushDir = toyVx > 0.0f ? 1 : -1;
        toyY = beetleTop - (float)spec.radius - 1.0f;
        if (toyY < TOY_TANK_TOP + spec.radius) toyY = (float)(TOY_TANK_TOP + spec.radius);
        toyVx = fabsf(toyVx) < 28.0f ? (float)pushDir * 28.0f : toyVx * 0.88f;
        toyVy = -fmaxf(fabsf(toyVy) * 0.72f, spec.liftImpulse * 0.42f);
        toySpin = (float)pushDir * (fabsf(toySpin) + 5.0f);
        toyNoCatchUntilMs = nowMs + TOY_NO_CATCH_AFTER_REBOUND_MS;
        return;
    }
    bool betweenBeetles = (toyX > (float)min(bugX, visitor.x) &&
                           toyX < (float)max(bugX, visitor.x));
    bool nearBack = fabsf(toyX - visitor.x) < beetleHalfW * 0.55f;
    if (betweenBeetles || nearBack) {
        int rollDir = visitor.x >= bugX ? 1 : -1;
        toyX = (float)visitor.x + rollDir * (beetleHalfW * 0.65f + (float)spec.radius);
        if (toyX < left) toyX = left;
        if (toyX > right) toyX = right;
        toyY = beetleTop - (float)spec.radius - 1.0f;
        if (toyY < TOY_TANK_TOP + spec.radius) toyY = (float)(TOY_TANK_TOP + spec.radius);
        toyVx = (float)(rollDir * random(58, 112));
        toyVy = (float)random(24, 56);
        toySpin = (float)(rollDir * random(6, 13));
        toyNoCatchUntilMs = nowMs + TOY_NO_CATCH_AFTER_REBOUND_MS;
        return;
    }

    toyX = pushDir > 0 ? beetleRight + spec.radius + 1.0f
                       : beetleLeft - spec.radius - 1.0f;
    if (toyX < left) toyX = left;
    if (toyX > right) toyX = right;
    float speed = sqrtf(toyVx * toyVx + toyVy * toyVy);
    bool toyMovingIntoVisitor = (pushDir > 0 && toyVx < -20.0f) ||
                                (pushDir < 0 && toyVx > 20.0f);
    bool canReact = nowMs >= toyNoCatchUntilMs &&
                    nowMs - toyLastHitMs >= TOY_HIT_COOLDOWN_MS &&
                    speed <= TOY_CATCH_MAX_SPEED &&
                    !toyMovingIntoVisitor;
    if (canReact) {
        startVisitorToyCharge(nowMs, pushDir);
    } else {
        deflectToyFromVisitor(nowMs, pushDir);
    }
}

bool TerrariumScene::startToyButtonInteraction(uint32_t nowMs, bool allowVisitorTarget) {
    if (!isToyEnabled()) {
        Serial.println("[Terrarium] B toy interaction skipped: toy disabled/poke style");
        return false;
    }
    if (toyEntryActive) {
        Serial.println("[Terrarium] B toy interaction consumed: entry active");
        return true;
    }
    toyType = toyTypeForStyle(GameEngine::ins().getToyStyle());
    ToyButtonInteraction interaction = toyButtonInteractionForStyle(GameEngine::ins().getToyStyle());
    bool targetVisitor = allowVisitorTarget &&
                         GameEngine::ins().isVisitHost() &&
                         chooseVisitorForHostInteraction();
    if (VISITOR_DEBUG_LOGS) {
        Serial.printf("[Terrarium] B toy interaction style=%u interaction=%u targetVisitor=%u\n",
                      GameEngine::ins().getToyStyle(),
                      (uint8_t)interaction,
                      targetVisitor ? 1 : 0);
    }
    int interactionX = hostInteractionTargetX(targetVisitor);
    float interactionScale = hostInteractionTargetScale(targetVisitor);
    switch (interaction) {
        case ToyButtonInteraction::THROW_ARC:
            startToyArcThrow(nowMs, interactionX, interactionScale, targetVisitor);
            return true;
        case ToyButtonInteraction::DROP_DOWN:
            startToyDrop(nowMs, interactionX, interactionScale, targetVisitor);
            return true;
        case ToyButtonInteraction::POKE:
        default:
            return false;
    }
}

void TerrariumScene::startToyArcThrow(uint32_t nowMs, int targetX, float targetScale, bool targetVisitor) {
    if (!isToyEnabled()) return;
    const ToySpec& spec = currentToySpec();
    toyEntryTargetX = targetX;
    toyEntryTargetVisitor = targetVisitor;
    toyVisible = false;
    toyCharging = false;
    toyEntryActive = true;
    toyEntryInteraction = ToyButtonInteraction::THROW_ARC;
    toyEntryStartMs = nowMs;
    toyThrowFromX = 120 + random(-42, 43);
    toyThrowFromY = Hal::DISPLAY_H + 18;
    toyThrowTargetX = targetX + random(-24, 25);
    toyThrowTargetY = GROUND_Y -
                      (int)(TOY_BEETLE_HIT_TOP * targetScale + (float)spec.radius - 1.0f) +
                      random(-2, 3);
    if (toyThrowTargetY < TOY_TANK_TOP + spec.radius) toyThrowTargetY = TOY_TANK_TOP + spec.radius;
    if (toyThrowTargetY > TOY_TANK_BOTTOM - spec.radius) toyThrowTargetY = TOY_TANK_BOTTOM - spec.radius;
    toyThrowArcH = random(48, 83);
    toyThrowCurveX = random(-28, 29);
    toyX = (float)toyThrowTargetX;
    toyY = (float)(TOY_TANK_BOTTOM - spec.radius);
    toyVx = 0.0f;
    toyVy = 0.0f;
    toySpin = 0.0f;
    toyNoCatchUntilMs = nowMs + TOY_NO_CATCH_AFTER_HIT_MS;
}

void TerrariumScene::startToyDrop(uint32_t nowMs, int targetX, float targetScale, bool targetVisitor) {
    if (!isToyEnabled()) return;
    const ToySpec& spec = currentToySpec();
    toyEntryTargetX = targetX;
    toyEntryTargetVisitor = targetVisitor;
    toyVisible = false;
    toyCharging = false;
    toyEntryActive = true;
    toyEntryInteraction = ToyButtonInteraction::DROP_DOWN;
    toyEntryStartMs = nowMs;
    toyThrowTargetX = targetX + random(-18, 19);
    toyThrowTargetY = GROUND_Y -
                      (int)(TOY_BEETLE_HIT_TOP * targetScale + (float)spec.radius - 1.0f) +
                      random(-2, 3);
    if (toyThrowTargetY < TOY_TANK_TOP + spec.radius) toyThrowTargetY = TOY_TANK_TOP + spec.radius;
    if (toyThrowTargetY > TOY_TANK_BOTTOM - spec.radius) toyThrowTargetY = TOY_TANK_BOTTOM - spec.radius;
    toyThrowFromX = toyThrowTargetX + random(-8, 9);
    toyThrowFromY = TOY_TANK_TOP - spec.radius - random(14, 27);
    toyThrowArcH = 0;
    toyThrowCurveX = random(-7, 8);
    toyX = (float)toyThrowTargetX;
    toyY = (float)(TOY_TANK_BOTTOM - spec.radius);
    toyVx = 0.0f;
    toyVy = 0.0f;
    toySpin = 0.0f;
    toyNoCatchUntilMs = nowMs + TOY_NO_CATCH_AFTER_HIT_MS;
}

void TerrariumScene::finishToyEntry(uint32_t nowMs) {
    const ToySpec& spec = currentToySpec();
    ToyButtonInteraction finishedInteraction = toyEntryInteraction;
    toyEntryActive = false;
    toyVisible = true;
    toyX = (float)toyThrowTargetX;
    toyY = (float)toyThrowTargetY;
    if (toyY < TOY_TANK_TOP + spec.radius) toyY = TOY_TANK_TOP + spec.radius;
    if (toyY > TOY_TANK_BOTTOM - spec.radius) toyY = TOY_TANK_BOTTOM - spec.radius;

    int pushDir = toyThrowTargetX >= toyThrowFromX ? 1 : -1;
    if (abs(toyThrowTargetX - toyThrowFromX) < 4) {
        pushDir = toyX >= toyEntryTargetX ? 1 : -1;
    }
    if (finishedInteraction == ToyButtonInteraction::DROP_DOWN) {
        toyVx = (float)(pushDir * random(80, 151));
        toyVy = -(float)random(90, 151);
        toySpin = (float)(pushDir * random(5, 11));
    } else {
        toyVx = (float)(pushDir * random(160, 251));
        toyVy = -(float)random(110, 181);
        toySpin = (float)(pushDir * random(8, 15));
    }
    toyLastHitMs = nowMs;
    toyNoCatchUntilMs = nowMs + TOY_NO_CATCH_AFTER_HIT_MS;
    toyLastUpdateMs = nowMs;
    bool entryHit = false;
    if (toyEntryTargetVisitor && visitor.active && !visitor.falling) {
        float scale = visitorAdultScale();
        float beetleHalfW = TOY_BEETLE_HIT_HALF_W * scale;
        float beetleTop = (float)visitor.targetY - TOY_BEETLE_HIT_TOP * scale;
        float beetleBottom = (float)visitor.targetY - TOY_BEETLE_HIT_BOTTOM;
        float beetleLeft = (float)visitor.x - beetleHalfW;
        float beetleRight = (float)visitor.x + beetleHalfW;
        if (circleHitsRect(toyX, toyY, (float)spec.radius,
                           beetleLeft, beetleTop, beetleRight, beetleBottom)) {
            reactVisitorToToyImpact(nowMs, pushDir);
            reboundToyFromEntryImpact(nowMs, pushDir, beetleTop, spec);
            entryHit = true;
        }
        if (!entryHit) {
            guideVisitorWalkTo(toyThrowTargetX, nowMs);
        }
    } else {
        Bug& bug = GameEngine::ins().getBug();
        if (isMobileBeetleStage(bug.getStage()) && !bug.isDead()) {
            float scale = bug.getAdultScale();
            float beetleHalfW = TOY_BEETLE_HIT_HALF_W * scale;
            float beetleTop = GROUND_Y - TOY_BEETLE_HIT_TOP * scale;
            float beetleBottom = GROUND_Y - TOY_BEETLE_HIT_BOTTOM;
            float beetleLeft = (float)bugX - beetleHalfW;
            float beetleRight = (float)bugX + beetleHalfW;
            if (circleHitsRect(toyX, toyY, (float)spec.radius,
                               beetleLeft, beetleTop, beetleRight, beetleBottom)) {
                reactLocalToToyImpact(nowMs, pushDir);
                reboundToyFromEntryImpact(nowMs, pushDir, beetleTop, spec);
                entryHit = true;
            }
        }
    }

    alertUntilMs = nowMs + random(ALERT_MIN_MS, ALERT_MAX_MS + 1);
}

void TerrariumScene::drawToyBall(int centerX, int centerY, int radius, uint8_t phase) {
    if (radius < 3) radius = 3;
    int x = centerX - radius;
    int y = centerY - radius;
    LGFX_Sprite& canvas = Hal::ins().canvas();
    PixelRenderer::fillRect(x, y, radius * 2, radius * 2, PixelRenderer::WHITE);
    canvas.drawRect(x, y, radius * 2, radius * 2, PixelRenderer::BLACK);

    int patch = radius / 2;
    if (patch < 2) patch = 2;
    int small = radius / 3;
    if (small < 1) small = 1;
    PixelRenderer::fillRect(centerX - patch / 2, centerY - patch / 2,
                            patch, patch, PixelRenderer::BLACK);
    if (phase == 0 || phase == 2) {
        PixelRenderer::fillRect(x + 1, y + 1, small, small, PixelRenderer::BLACK);
        PixelRenderer::fillRect(centerX + radius - small - 1, centerY + radius - small - 1,
                                small, small, PixelRenderer::BLACK);
    } else {
        PixelRenderer::fillRect(centerX + radius - small - 1, y + 1,
                                small, small, PixelRenderer::BLACK);
        PixelRenderer::fillRect(x + 1, centerY + radius - small - 1,
                                small, small, PixelRenderer::BLACK);
    }
}

void TerrariumScene::drawToyEntry() {
    if (!toyEntryActive) return;
    uint32_t duration = toyEntryDurationMs();
    uint32_t elapsed = Hal::ins().millis() - toyEntryStartMs;
    if (elapsed > duration) elapsed = duration;
    float t = duration == 0 ? 1.0f : (float)elapsed / (float)duration;
    float arc = 4.0f * t * (1.0f - t);
    int x = 0;
    int y = 0;
    int radius = currentToySpec().radius;

    if (toyEntryInteraction == ToyButtonInteraction::DROP_DOWN) {
        float easeIn = t * t;
        x = (int)roundf(toyThrowFromX + (toyThrowTargetX - toyThrowFromX) * t +
                        toyThrowCurveX * arc);
        y = (int)roundf(toyThrowFromY + (toyThrowTargetY - toyThrowFromY) * easeIn);
        radius += (int)(2.0f * (1.0f - t));
    } else {
        float depthEase = 1.0f - (1.0f - t) * (1.0f - t);
        x = (int)roundf(toyThrowFromX + (toyThrowTargetX - toyThrowFromX) * t +
                        toyThrowCurveX * arc);
        y = (int)roundf(toyThrowFromY + (toyThrowTargetY - toyThrowFromY) * t -
                        toyThrowArcH * arc);
        radius = (int)(18.0f + (float)(currentToySpec().radius - 18) * depthEase);
    }

    uint8_t phase = (uint8_t)((elapsed / 80) & 0x03);
    drawToyBall(x, y, radius, phase);
}

void TerrariumScene::drawToy() {
    if (!isToyEnabled() || !toyVisible) return;
    const ToySpec& spec = currentToySpec();
    int radius = spec.radius;
    uint8_t phase = (uint8_t)((int)(toyAngle * 2.0f) & 0x03);
    drawToyBall((int)roundf(toyX), (int)roundf(toyY), radius, phase);
}

