void TerrariumScene::updateVisitor(uint32_t nowMs) {
    if (!visitor.active) return;
    if (nowMs >= visitor.untilMs) {
        visitor.active = false;
        return;
    }

    if (visitor.falling) {
        uint32_t elapsed = nowMs - visitor.startMs;
        if (elapsed >= VISITOR_DROP_MS) {
            visitor.falling = false;
            visitor.y = visitor.targetY;
            visitor.actor.stateTimer = 0;
            visitor.nextStepMs = nowMs + VISITOR_STEP_MS;
            logVisitorState("landed", nowMs);
        } else {
            float t = (float)elapsed / (float)VISITOR_DROP_MS;
            float ease = 1.0f - (1.0f - t) * (1.0f - t);
            visitor.y = (int)(visitor.fromY + (visitor.targetY - visitor.fromY) * ease);
        }
    } else {
        uint32_t stepMs = GameEngine::ins().isVisitHost() ? VISITOR_HOST_STEP_MS : VISITOR_STEP_MS;
        uint32_t intentStepMs = GameEngine::ins().isVisitHost() ? VISITOR_HOST_INTENT_STEP_MS : VISITOR_INTENT_STEP_MS;
        visitor.y = visitor.targetY;
        if (visitor.actor.state != AdultState::TURN && visitor.turning) {
            logVisitorState("clear-stale-turning", nowMs);
            visitor.turning = false;
        }
        visitor.actor.stateTimer++;
        if (!visitorStateLogInitialized ||
            visitor.actor.state != visitorLastLoggedState ||
            (visitor.actor.state == AdultState::TURN &&
             nowMs - lastVisitorStateLogMs >= 700)) {
            logVisitorState("tick", nowMs);
        }
        if (visitor.actor.state == AdultState::THREATEN) {
            if (nowMs >= visitor.actor.threatenEndMs) {
                visitor.actor.state = AdultState::IDLE;
                visitor.actor.stateTimer = 0;
                scheduleVisitorWander(nowMs);
                logVisitorState("threaten-done", nowMs);
            }
            return;
        }
        if (visitor.actor.state == AdultState::ATTACK_DOWN) {
            return;
        }
        if (visitor.actor.state == AdultState::ATTACK_UP) {
            if (nowMs - visitorToyAttackStartMs >= TOY_ATTACK_UP_MS) {
                visitor.actor.state = AdultState::IDLE;
                visitor.actor.stateTimer = 0;
                scheduleVisitorWander(nowMs);
                logVisitorState("attack-done", nowMs);
            }
            return;
        }
        if (visitor.actor.state == AdultState::EAT) {
            if (visitor.actor.stateTimer >= visitor.actor.stateDuration) {
                visitor.actor.state = AdultState::IDLE;
                visitor.actor.stateTimer = 0;
                scheduleVisitorWander(nowMs);
                logVisitorState("eat-done", nowMs);
            }
            return;
        }
        if (nowMs >= visitor.nextStepMs) {
            if (visitor.turning) {
                if (nowMs - visitor.turnStartMs >= VISITOR_TURN_MS) {
                    visitor.turning = false;
                    visitor.faceRight = visitor.turnTargetFaceRight;
                    visitor.actor.stateTimer = 0;
                    if (visitor.actor.slideAfterTurn) {
                        visitor.actor.slideAfterTurn = false;
                        visitor.actor.state = AdultState::SLIDE;
                    } else if (visitor.actor.climbAfterTurn) {
                        visitor.actor.climbAfterTurn = false;
                        visitor.actor.state = AdultState::CLIMB;
                    } else if (visitor.actor.walkAfterTurn &&
                               abs(visitor.x - visitor.targetX) > 1) {
                        visitor.actor.state = AdultState::WALK;
                    } else {
                        visitor.actor.state = AdultState::IDLE;
                    }
                    logVisitorState("turn-done", nowMs);
                } else {
                    visitor.nextStepMs = nowMs + stepMs;
                    return;
                }
            }

            if (visitor.actor.state == AdultState::SLIDE) {
                int dx = (visitor.actor.slideTargetX > visitor.x) ? 1 : -1;
                visitor.faceRight = dx > 0;
                visitor.x += dx * TILT_SLIDE_STEP;
                if (visitor.x < MIN_X) visitor.x = MIN_X;
                if (visitor.x > MAX_X) visitor.x = MAX_X;
                if (visitor.x == visitor.actor.slideTargetX ||
                    (dx > 0 && visitor.x >= visitor.actor.slideTargetX) ||
                    (dx < 0 && visitor.x <= visitor.actor.slideTargetX)) {
                    bool needFaceRight = visitor.actor.tiltHighSideIsRight;
                    if (needFaceRight != visitor.faceRight) {
                        beginActorTurn(visitor.actor, needFaceRight, true, nowMs, VISITOR_TURN_MS);
                        visitor.actor.climbAfterTurn = true;
                    } else {
                        visitor.actor.state = AdultState::CLIMB;
                        visitor.actor.stateTimer = 0;
                        logVisitorState("slide-to-climb", nowMs);
                    }
                }
                visitor.nextStepMs = nowMs + stepMs;
                return;
            }

            if (visitor.actor.state == AdultState::CLIMB) {
                if (abs(visitor.actor.climbTargetX - visitor.x) <= 1) {
                    visitor.actor.state = AdultState::IDLE;
                    visitor.actor.stateTimer = 0;
                    scheduleVisitorWander(nowMs);
                    visitor.nextStepMs = nowMs + stepMs;
                    logVisitorState("climb-done", nowMs);
                    return;
                }
                int dx = (visitor.actor.climbTargetX > visitor.x) ? 1 : -1;
                visitor.faceRight = dx > 0;
                if ((visitor.actor.stateTimer % TILT_CLIMB_SPEED_INTERVAL) == 0) {
                    visitor.x += dx;
                    if (visitor.x < MIN_X) visitor.x = MIN_X;
                    if (visitor.x > MAX_X) visitor.x = MAX_X;
                }
                if (abs(visitor.actor.climbTargetX - visitor.x) <= 1) {
                    visitor.actor.state = AdultState::IDLE;
                    visitor.actor.stateTimer = 0;
                    scheduleVisitorWander(nowMs);
                    logVisitorState("climb-done", nowMs);
                }
                visitor.nextStepMs = nowMs + stepMs;
                return;
            }

            if (abs(visitor.x - visitor.targetX) <= 1) {
                if (visitor.nextWanderMs == 0) {
                    visitor.actor.state = AdultState::IDLE;
                    scheduleVisitorWander(nowMs);
                    logVisitorState("arrived-idle", nowMs);
                }
                if (nowMs < visitor.nextWanderMs) {
                    visitor.nextStepMs = nowMs + stepMs;
                    return;
                }
                visitor.nextWanderMs = 0;
                chooseVisitorTarget();
            }
            if (visitor.x < visitor.targetX) {
                if (!visitor.faceRight) {
                    beginActorTurn(visitor.actor, true, true, nowMs, VISITOR_TURN_MS);
                    visitor.nextStepMs = nowMs + stepMs;
                    return;
                }
                visitor.actor.state = AdultState::WALK;
                visitor.x++;
            } else if (visitor.x > visitor.targetX) {
                if (visitor.faceRight) {
                    beginActorTurn(visitor.actor, false, true, nowMs, VISITOR_TURN_MS);
                    visitor.nextStepMs = nowMs + stepMs;
                    return;
                }
                visitor.actor.state = AdultState::WALK;
                visitor.x--;
            }
            visitor.nextStepMs = nowMs + (nowMs < visitorIntentUntilMs ? intentStepMs : stepMs);
        }
    }
}

void TerrariumScene::chooseVisitorTarget() {
    int left = MIN_X + 8;
    int right = MAX_X - 8;
    if (left >= right) {
        beginActorWalkTo(visitor.actor, visitor.x);
        visitor.actor.state = AdultState::IDLE;
        logVisitorState("choose-idle", Hal::ins().millis());
        return;
    }

    int next = random(left, right + 1);
    if (abs(next - bugX) < 32) {
        next = bugX < (left + right) / 2 ? random((left + right) / 2, right + 1)
                                         : random(left, (left + right) / 2 + 1);
    }
    beginActorWalkTo(visitor.actor, next);
    visitor.actor.state = AdultState::WALK;
    logVisitorState("choose-target", Hal::ins().millis());
}

void TerrariumScene::scheduleVisitorWander(uint32_t nowMs) {
    visitor.nextWanderMs = nowMs + random(VISITOR_IDLE_MIN_MS, VISITOR_IDLE_MAX_MS + 1);
}

void TerrariumScene::logVisitorState(const char* reason, uint32_t nowMs) {
    visitorLastLoggedState = visitor.actor.state;
    visitorStateLogInitialized = true;
    lastVisitorStateLogMs = nowMs;
    if (!VISITOR_DEBUG_LOGS) return;

    int32_t nextStepIn = (int32_t)(visitor.nextStepMs - nowMs);
    int32_t nextWanderIn = visitor.nextWanderMs == 0 ? 0 : (int32_t)(visitor.nextWanderMs - nowMs);
    int32_t intentIn = visitorIntentUntilMs == 0 ? 0 : (int32_t)(visitorIntentUntilMs - nowMs);
    Serial.printf("[Visitor] %s state=%s x=%d target=%d face=%u turnTarget=%u turning=%u fall=%u timer=%lu dur=%lu nextStep=%ld wander=%ld intent=%ld\n",
                  reason,
                  adultStateName(visitor.actor.state),
                  visitor.x,
                  visitor.targetX,
                  visitor.faceRight ? 1 : 0,
                  visitor.turnTargetFaceRight ? 1 : 0,
                  visitor.turning ? 1 : 0,
                  visitor.falling ? 1 : 0,
                  (unsigned long)visitor.actor.stateTimer,
                  (unsigned long)visitor.actor.stateDuration,
                  (long)nextStepIn,
                  (long)nextWanderIn,
                  (long)intentIn);
}

void TerrariumScene::guideVisitorWalkTo(int targetX, uint32_t nowMs) {
    beginActorWalkTo(visitor.actor, targetX);
    visitor.nextWanderMs = 0;
    visitorIntentUntilMs = nowMs + VISITOR_INTENT_MS;

    if (visitor.actor.state == AdultState::TURN ||
        visitor.actor.state == AdultState::THREATEN ||
        visitor.actor.state == AdultState::ATTACK_DOWN ||
        visitor.actor.state == AdultState::ATTACK_UP ||
        visitor.actor.state == AdultState::EAT) {
        logVisitorState("guide-defer", nowMs);
        return;
    }

    visitor.actor.state = AdultState::WALK;
    visitor.actor.stateTimer = 0;
    visitor.turning = false;
    visitor.nextStepMs = nowMs;
    logVisitorState("guide-walk", nowMs);
}

void TerrariumScene::beginActorTurn(AdultBeetleActor& actor, bool targetFaceRight,
                                    bool continueWalking, uint32_t timer,
                                    uint32_t duration) {
    actor.state = AdultState::TURN;
    actor.turnTargetFaceRight = targetFaceRight;
    actor.walkAfterTurn = continueWalking;
    actor.slideAfterTurn = false;
    actor.climbAfterTurn = false;
    if (!continueWalking) actor.walkTargetIsRest = false;
    actor.turnFrameIndex = random(HerculesAdultSprites::TURN_FRAME_COUNT);
    actor.stateTimer = 0;
    actor.stateDuration = duration;
    actor.turning = true;
    actor.turnStartMs = timer;
    if (&actor == &visitor.actor) {
        logVisitorState("turn-start", timer);
    }
}

void TerrariumScene::beginActorWalkTo(AdultBeetleActor& actor, int x) {
    actor.targetX = x;
    if (actor.targetX < MIN_X) actor.targetX = MIN_X;
    if (actor.targetX > MAX_X) actor.targetX = MAX_X;
}

bool TerrariumScene::chooseVisitorForHostInteraction() const {
    bool available = visitor.active && !visitor.falling;
    bool selected = available && random(2) == 0;
    if (VISITOR_DEBUG_LOGS) {
        Serial.printf("[Visitor] choose-interaction available=%u selected=%u x=%d state=%s\n",
                      available ? 1 : 0,
                      selected ? 1 : 0,
                      visitor.x,
                      adultStateName(visitor.actor.state));
    }
    return selected;
}

int TerrariumScene::hostInteractionTargetX(bool targetVisitor) const {
    return targetVisitor ? visitor.x : bugX;
}

float TerrariumScene::hostInteractionTargetScale(bool targetVisitor) const {
    return targetVisitor ? visitorAdultScale() : GameEngine::ins().getBug().getAdultScale();
}

void TerrariumScene::reactVisitorToHostPoke(uint32_t nowMs) {
    int dir = pokeFingerFromRight ? -1 : 1;
    visitor.x += dir * 5;
    if (visitor.x < MIN_X) visitor.x = MIN_X;
    if (visitor.x > MAX_X) visitor.x = MAX_X;
    visitor.faceRight = pokeFingerFromRight;
    beginActorWalkTo(visitor.actor, visitor.x);
    visitor.actor.state = AdultState::THREATEN;
    visitor.actor.stateTimer = 0;
    visitor.actor.stateDuration = POKE_REACTION_MS;
    visitor.actor.threatenStartMs = nowMs;
    visitor.actor.threatenEndMs = nowMs + POKE_REACTION_MS;
    visitorIntentUntilMs = nowMs + VISITOR_INTENT_MS;
    visitor.turning = false;
    visitor.nextWanderMs = visitor.actor.threatenEndMs;
    logVisitorState("poke-threaten", nowMs);
}

void TerrariumScene::applyTiltToVisitor(TiltDir dir, float magnitude) {
    if (!visitor.active || visitor.falling) return;
    if (visitor.actor.state == AdultState::EAT) return;
    if (visitor.actor.state == AdultState::TURN) {
        logVisitorState("tilt-defer-turn", Hal::ins().millis());
        return;
    }
    if (visitor.actor.state == AdultState::ATTACK_DOWN ||
        visitor.actor.state == AdultState::ATTACK_UP) {
        return;
    }
    if (visitor.actor.state == AdultState::THREATEN &&
        magnitude <= TILT_SLIDE_THRESHOLD_G) {
        return;
    }

    uint32_t nowMs = Hal::ins().millis();
    bool lowIsLeft = (dir == TiltDir::LEFT);
    visitor.actor.tiltHighSideIsRight = !lowIsLeft;

    int highTarget = visitor.x + (visitor.actor.tiltHighSideIsRight
                                  ? TILT_CLIMB_DISTANCE
                                  : -TILT_CLIMB_DISTANCE);
    if (highTarget < MIN_X) highTarget = MIN_X;
    if (highTarget > MAX_X) highTarget = MAX_X;
    visitor.actor.climbTargetX = highTarget;

    visitor.nextWanderMs = 0;
    visitor.nextStepMs = nowMs;
    visitorIntentUntilMs = nowMs + VISITOR_INTENT_MS;

    if (magnitude > TILT_SLIDE_THRESHOLD_G) {
        int lowTarget = visitor.x + (lowIsLeft ? -TILT_SLIDE_DISTANCE : TILT_SLIDE_DISTANCE);
        if (lowTarget < MIN_X) lowTarget = MIN_X;
        if (lowTarget > MAX_X) lowTarget = MAX_X;
        visitor.actor.slideTargetX = lowTarget;

        bool needFaceRight = !visitor.actor.tiltHighSideIsRight;
        if (needFaceRight != visitor.faceRight) {
            beginActorTurn(visitor.actor, needFaceRight, true, nowMs, VISITOR_TURN_MS);
            visitor.actor.slideAfterTurn = true;
            logVisitorState("tilt-slide-turn", nowMs);
        } else {
            visitor.actor.state = AdultState::SLIDE;
            visitor.actor.stateTimer = 0;
            visitor.turning = false;
            logVisitorState("tilt-slide", nowMs);
        }
        return;
    }

    bool needFaceRight = visitor.actor.tiltHighSideIsRight;
    if (needFaceRight != visitor.faceRight) {
        beginActorTurn(visitor.actor, needFaceRight, true, nowMs, VISITOR_TURN_MS);
        visitor.actor.climbAfterTurn = true;
        logVisitorState("tilt-climb-turn", nowMs);
    } else {
        visitor.actor.state = AdultState::CLIMB;
        visitor.actor.stateTimer = 0;
        visitor.turning = false;
        logVisitorState("tilt-climb", nowMs);
    }
}

void TerrariumScene::updateVisitGuestLink(uint32_t nowMs) {
    BattleLink& link = BattleLink::ins();
    if (!link.isBattlePeerSet()) {
        GameEngine::ins().clearVisitSession();
        visitRecallConfirm = false;
        Serial.println("[Terrarium] Visit auto recalled: no peer");
        return;
    }

    visit_status_t status;
    if (link.takeReceivedVisitStatus(status)) {
        GameEngine::ins().syncVisitGuestRewards(status.guest_eat_count,
                                                status.guest_play_count);
        if ((status.flags & 1) == 0 || status.remaining_s == 0) {
            GameEngine::ins().clearVisitSession();
            visitRecallConfirm = false;
            visitPingInFlight = false;
            Serial.println("[Terrarium] Visit ended by host status");
            return;
        }
        GameEngine::ins().syncVisitTiming((uint32_t)status.remaining_s * 1000UL,
                                          (uint32_t)status.duration_s * 1000UL,
                                          status.speed_x10);
        if (GameEngine::ins().setGameSpeedFromX10(status.speed_x10)) {
            GameEngine::ins().saveSettingsSnapshot();
            Serial.printf("[Terrarium] Visit guest synced host game speed=%u\n",
                          status.speed_x10);
        }
        visitPingFailures = 0;
        lastVisitPingMs = nowMs;
        lastVisitStatusRxMs = nowMs;
    }

    visit_eat_result_t eatResult;
    if (link.takeReceivedVisitEatResult(eatResult)) {
        if (eatResult.success && eatResult.hunger_gain > 0) {
            GameEngine::ins().getBug().modHunger((int8_t)eatResult.hunger_gain);
            GameEngine::ins().recordVisitLocalEat();
            GameEngine::ins().forceSave();
            visitEatRetryAfterMs = nowMs + 2000;
            Serial.printf("[Terrarium] Visit eat success gain=%u localHun=%u\n",
                          eatResult.hunger_gain,
                          GameEngine::ins().getBug().getHunger());
        } else {
            visitEatRetryAfterMs = nowMs + VISIT_EAT_FAIL_RETRY_MS;
            Serial.println("[Terrarium] Visit eat failed");
        }
    }

    if (visitPingInFlight && !link.isSending()) {
        bool ok = link.takeLastSendSuccess();
        visitPingInFlight = false;
        lastVisitPingMs = nowMs;
        if (ok) {
            visitPingFailures = 0;
        } else if (++visitPingFailures >= VISIT_PING_MAX_FAILURES) {
            GameEngine::ins().clearVisitSession();
            visitRecallConfirm = false;
            Serial.println("[Terrarium] Visit auto recalled: host unreachable");
            return;
        }
    }

    if (nowMs - lastVisitStatusRxMs >= VISIT_STATUS_TIMEOUT_MS) {
        GameEngine::ins().clearVisitSession();
        visitRecallConfirm = false;
        visitPingInFlight = false;
        Serial.println("[Terrarium] Visit auto recalled: host status timeout");
        return;
    }

    if (!visitPingInFlight &&
        nowMs - lastVisitPingMs >= VISIT_PING_INTERVAL_MS &&
        link.sendVisitPing(GameEngine::ins().getBug().getHunger(),
                           GameEngine::ins().getBug().getMot())) {
        visitPingInFlight = true;
    }

    Bug& bug = GameEngine::ins().getBug();
    if (bug.getHunger() < VISIT_EAT_HUNGER_THRESHOLD &&
        nowMs >= visitEatRetryAfterMs &&
        nowMs - lastVisitEatIntentMs >= VISIT_EAT_INTENT_INTERVAL_MS &&
        !link.isSending() &&
        link.sendVisitIntent(VISIT_INTENT_EAT)) {
        lastVisitEatIntentMs = nowMs;
        return;
    }

    if (nowMs >= nextVisitPlayIntentMs && !link.isSending() &&
        link.sendVisitIntent(VISIT_INTENT_PLAY)) {
        nextVisitPlayIntentMs = nowMs + random(VISIT_PLAY_INTENT_MIN_MS,
                                               VISIT_PLAY_INTENT_MAX_MS + 1);
    }
}

void TerrariumScene::updateVisitHostLink(uint32_t nowMs) {
    BattleLink& link = BattleLink::ins();
    uint8_t remoteHunger = 0;
    uint8_t remoteMotivation = 0;
    while (link.takeReceivedVisitVitals(remoteHunger, remoteMotivation)) {
        GameEngine::ins().setVisitRemoteVitals(remoteHunger, remoteMotivation);
    }

    uint8_t intent = 0;
    while (link.takeReceivedVisitIntent(intent)) {
        applyVisitIntent(intent, nowMs);
    }
    updateVisitorEating(nowMs);

    if (!link.isBattlePeerSet() || link.isSending()) return;
    if (flushVisitEatResult()) return;
    if (nowMs - lastVisitStatusMs < VISIT_STATUS_INTERVAL_MS) return;
    if (link.sendVisitStatus(GameEngine::ins().getVisitRemainingMs(),
                             GameEngine::ins().getVisitDurationMs(),
                             GameEngine::ins().getGameSpeedX10(),
                             GameEngine::ins().isVisitHost(),
                             GameEngine::ins().getVisitRemoteEatCount(),
                             GameEngine::ins().getVisitRemotePlayCount())) {
        lastVisitStatusMs = nowMs;
    }
}

void TerrariumScene::applyVisitIntent(uint8_t intent, uint32_t nowMs) {
    if (!visitor.active || visitor.falling) return;
    if (intent == VISIT_INTENT_EAT) {
        visitorEatRequested = true;
        visitorIntentUntilMs = nowMs + VISITOR_INTENT_MS;
        if (GameEngine::ins().getBug().hasFoodInTray() &&
            GameEngine::ins().getBug().getFoodAmount() > 0) {
            guideVisitorWalkTo(FOOD_X, nowMs);
        } else {
            queueVisitEatResult(false, 0, GameEngine::ins().getVisitRemoteBug().hunger, 0);
            visitorEatRequested = false;
            logVisitorState("intent-eat-no-food", nowMs);
        }
        Serial.println("[Terrarium] Visit eat intent received");
        return;
    }
    if (intent != VISIT_INTENT_PLAY) return;
    visitorIntentUntilMs = nowMs + VISITOR_INTENT_MS;
    if (toyVisible) {
        guideVisitorWalkTo((int)roundf(toyX), nowMs);
    } else {
        chooseVisitorTarget();
        visitor.nextStepMs = nowMs;
        logVisitorState("intent-play-no-toy", nowMs);
    }
    Serial.println("[Terrarium] Visit intent applied");
}

void TerrariumScene::updateVisitorEating(uint32_t nowMs) {
    if (!visitorEatRequested || !visitor.active || visitor.falling) return;

    Bug& bug = GameEngine::ins().getBug();
    if (!bug.hasFoodInTray() || bug.getFoodAmount() == 0) {
        queueVisitEatResult(false, 0, GameEngine::ins().getVisitRemoteBug().hunger, 0);
        visitorEatRequested = false;
        visitor.actor.state = AdultState::IDLE;
        logVisitorState("eat-no-food", nowMs);
        return;
    }

    guideVisitorWalkTo(FOOD_X, nowMs);
    if (abs(visitor.x - FOOD_X) > VISITOR_EAT_DISTANCE_PX) return;

    uint8_t gain = 0;
    FoodType foodType = bug.getFoodInTrayType();
    if (!bug.consumeTrayBiteForVisitor(gain, foodType)) {
        queueVisitEatResult(false, 0, GameEngine::ins().getVisitRemoteBug().hunger, (uint8_t)foodType);
        visitorEatRequested = false;
        visitor.actor.state = AdultState::IDLE;
        logVisitorState("eat-failed", nowMs);
        return;
    }

    uint8_t newHunger = GameEngine::ins().getVisitRemoteBug().hunger;
    uint16_t h = (uint16_t)newHunger + gain;
    newHunger = h > 100 ? 100 : (uint8_t)h;
    GameEngine::ins().setVisitRemoteHunger(newHunger);
    GameEngine::ins().recordVisitRemoteEat();
    queueVisitEatResult(true, gain, newHunger, (uint8_t)foodType);
    visitorEatRequested = false;
    visitor.actor.state = AdultState::EAT;
    visitor.actor.stateTimer = 0;
    visitor.actor.stateDuration = EAT_MIN_EXIT_FRAMES;
    visitor.faceRight = false;
    beginActorWalkTo(visitor.actor, visitor.x);
    logVisitorState("eat-start", nowMs);
    GameEngine::ins().forceSave();
    Serial.printf("[Terrarium] Visitor ate food=%u gain=%u newHun=%u\n",
                  (uint8_t)foodType, gain, newHunger);
}

void TerrariumScene::queueVisitEatResult(bool success, uint8_t hungerGain,
                                         uint8_t newGuestHunger, uint8_t foodType) {
    pendingVisitEatResult = true;
    pendingVisitEatSuccess = success;
    pendingVisitEatGain = hungerGain;
    pendingVisitEatHunger = newGuestHunger > 100 ? 100 : newGuestHunger;
    pendingVisitEatFood = foodType;
}

bool TerrariumScene::flushVisitEatResult() {
    if (!pendingVisitEatResult) return false;
    if (!BattleLink::ins().sendVisitEatResult(pendingVisitEatSuccess,
                                              pendingVisitEatGain,
                                              pendingVisitEatHunger,
                                              pendingVisitEatFood)) {
        return false;
    }
    pendingVisitEatResult = false;
    return true;
}

