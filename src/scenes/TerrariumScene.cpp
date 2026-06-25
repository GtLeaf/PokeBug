#include "TerrariumScene.h"
#include "../core/GameEngine.h"
#include "../core/UiStrings.h"
#include "../hardware/BattleLink.h"
#include "../hardware/Hal.h"
#include "../assets/MainSceneAssets.h"
#include "../assets/HerculesEggSprites.h"
#include "../assets/HerculesLarvaSprites.h"
#include "../assets/HerculesAdultSprites.h"
#include "../assets/HerculesPupaSprites.h"
#include "../assets/WoodAssets.h"
#include "../assets/BowlAssets.h"
#include "../assets/FoodAssets.h"
#include "../assets/ActionAssets.h"
#include <cmath>

const uint16_t TerrariumScene::PALETTE[4][2] = {
    { PixelRenderer::BROWN, PixelRenderer::DARK_BROWN },
    { 0x0400, 0x0200 },   // 深绿/暗绿
    { 0xFE00, 0xA600 },   // 金色/暗金
    { 0xE71C, 0x8010 },   // 白化淡紫/暗紫
};

namespace {

constexpr int PUPA_BOTTOM_MARGIN_PX = 0;
constexpr int PUPA_POKE_SHAKE_PX = 3;
constexpr bool VISITOR_DEBUG_LOGS = false;
constexpr float TOY_BEETLE_HIT_HALF_W = 34.0f;
constexpr float TOY_BEETLE_HIT_TOP = 44.0f;
constexpr float TOY_BEETLE_HIT_BOTTOM = 2.0f;

bool isMobileBeetleStage(Stage stage) {
    return stage == Stage::ADULT || stage == Stage::JUVENILE;
}

bool circleHitsRect(float cx, float cy, float radius,
                    float left, float top, float right, float bottom) {
    float closestX = cx;
    if (closestX < left) closestX = left;
    if (closestX > right) closestX = right;
    float closestY = cy;
    if (closestY < top) closestY = top;
    if (closestY > bottom) closestY = bottom;

    float dx = cx - closestX;
    float dy = cy - closestY;
    return dx * dx + dy * dy <= radius * radius;
}

const char* adultStateName(AdultState state) {
    switch (state) {
        case AdultState::IDLE:        return "IDLE";
        case AdultState::WALK:        return "WALK";
        case AdultState::EAT:         return "EAT";
        case AdultState::TURN:        return "TURN";
        case AdultState::SLIDE:       return "SLIDE";
        case AdultState::CLIMB:       return "CLIMB";
        case AdultState::REST:        return "REST";
        case AdultState::THREATEN:    return "THREATEN";
        case AdultState::ATTACK_DOWN: return "ATTACK_DOWN";
        case AdultState::ATTACK_UP:   return "ATTACK_UP";
    }
    return "?";
}

uint16_t adultHueMain(Temperament temperament) {
    switch (temperament) {
        case Temperament::BRUTE:     return 0xF800; // 深红
        case Temperament::SWIFT:     return 0x6B7D; // 灰蓝
        case Temperament::GIANT:     return 0xFD20; // 橙褐
        case Temperament::RESILIENT: return 0xFE00; // 金色
        case Temperament::BALANCED:  return 0xFFFF; // 白/浅灰
        case Temperament::SPIRIT:    return 0x07E0; // 青绿
    }
    return 0xF800;
}

uint16_t mixRgb565(uint16_t base, uint16_t mix, float mixRatio) {
    if (mixRatio < 0.0f) mixRatio = 0.0f;
    if (mixRatio > 1.0f) mixRatio = 1.0f;
    uint8_t baseR = (base >> 11) & 0x1F;
    uint8_t baseG = (base >> 5) & 0x3F;
    uint8_t baseB = base & 0x1F;
    uint8_t mixR = (mix >> 11) & 0x1F;
    uint8_t mixG = (mix >> 5) & 0x3F;
    uint8_t mixB = mix & 0x1F;
    uint8_t r = (uint8_t)(baseR * (1.0f - mixRatio) + mixR * mixRatio);
    uint8_t g = (uint8_t)(baseG * (1.0f - mixRatio) + mixG * mixRatio);
    uint8_t b = (uint8_t)(baseB * (1.0f - mixRatio) + mixB * mixRatio);
    return (uint16_t)((r << 11) | (g << 5) | b);
}

uint16_t brightenRgb565(uint16_t color, float factor) {
    uint8_t r = (color >> 11) & 0x1F;
    uint8_t g = (color >> 5) & 0x3F;
    uint8_t b = color & 0x1F;
    uint16_t rr = (uint16_t)(r * factor);
    uint16_t gg = (uint16_t)(g * factor);
    uint16_t bb = (uint16_t)(b * factor);
    if (rr > 0x1F) rr = 0x1F;
    if (gg > 0x3F) gg = 0x3F;
    if (bb > 0x1F) bb = 0x1F;
    return (uint16_t)((rr << 11) | (gg << 5) | bb);
}

uint16_t adultDepthColor(Temperament temperament, float ratio) {
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    uint16_t base = adultHueMain(temperament);
    if (ratio < 0.25f) {
        return mixRgb565(mixRgb565(base, PixelRenderer::GRAY, 0.5f), PixelRenderer::WHITE, 0.3f);
    }
    if (ratio < 0.50f) {
        return mixRgb565(base, PixelRenderer::GRAY, 0.3f);
    }
    if (ratio < 0.75f) {
        return base;
    }
    return brightenRgb565(base, 1.25f);
}

}

void TerrariumScene::onEnter() {
    animFrame = 0;
    resetPressStart = 0;
    resetting = false;
    visitRecallConfirm = false;
    visitPingInFlight = false;
    visitPingFailures = 0;
    lastVisitPingMs = Hal::ins().millis();
    lastVisitStatusMs = Hal::ins().millis();
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
        GameEngine::ins().setBowlStyle(0xFF);
        bug.setFoodTray(0, (FoodType)GameEngine::ins().getFoodStyle());
        GameEngine::ins().setWoodStyle(0xFF);
        bug.removeWood();
    }

    const TerrariumViewState& saved = GameEngine::ins().getTerrariumViewState();
    if (saved.valid && isMobileBeetleStage(bug.getStage()) && !bug.isDead()) {
        bugX = saved.bugX;
        bugY = saved.bugY;
        animFrame = saved.animFrame;
        adultState = saved.adultState <= (uint8_t)AdultState::ATTACK_UP ?
                     (AdultState)saved.adultState : AdultState::IDLE;
        faceRight = saved.faceRight;
        turnTargetFaceRight = saved.turnTargetFaceRight;
        walkAfterTurn = saved.walkAfterTurn;
        slideAfterTurn = saved.slideAfterTurn;
        climbAfterTurn = saved.climbAfterTurn;
        walkTargetIsRest = false;
        turnFrameIndex = saved.turnFrameIndex;
        targetX = saved.targetX;
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
    if (isMobileBeetleStage(bug.getStage()) && !bug.isDead()) {
        TerrariumViewState state;
        state.bugX = bugX;
        state.bugY = bugY;
        state.animFrame = animFrame;
        state.adultState = (uint8_t)adultState;
        state.faceRight = faceRight;
        state.turnTargetFaceRight = turnTargetFaceRight;
        state.walkAfterTurn = walkAfterTurn;
        state.slideAfterTurn = slideAfterTurn;
        state.climbAfterTurn = climbAfterTurn;
        // walkTargetIsRest 是短暂运行时意图，不持久化，恢复后按普通 WALK 继续。
        state.turnFrameIndex = turnFrameIndex;
        state.targetX = targetX;
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

        uint32_t nowMs = Hal::ins().millis();
        if (visitor.active && nowMs < visitor.untilMs) {
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
    animFrame = 0;
    adultState = AdultState::IDLE;
    faceRight = true;
    turnTargetFaceRight = true;
    walkAfterTurn = false;
    slideAfterTurn = false;
    climbAfterTurn = false;
    walkTargetIsRest = false;
    turnFrameIndex = 0;
    targetX = bugX;
    slideTargetX = bugX;
    climbTargetX = bugX;
    tiltHighSideIsRight = true;
    eatFrameInterval = 0;
    eatBitesThisSession = 0;
    eatLastBiteMs = 0;
    larvaState = LarvaState::IDLE;
    larvaStateStartMs = Hal::ins().millis();
    larvaStateDurationMs = random(LARVA_IDLE_MIN_MS, LARVA_IDLE_MAX_MS + 1);
    observedLarvaEatGameMs = GameEngine::ins().getBug().getLastEatTime();
    restResumeAllowedMs = 0;
    foodRefillGraceUntilMs = 0;
    alertUntilMs = 0;
    resetToy();
    mind.resetActivityTimer(Hal::ins().millis());
    stateTimer = 0;
    setIdleDuration();
}

void TerrariumScene::restoreVisitorFromViewState(const TerrariumViewState& saved) {
    visitor.active = false;
    if (!saved.visitorActive || saved.visitorRemainingMs == 0) return;

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

    if (!GameEngine::ins().hasPendingVisitBug()) return;

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
                    visitor.nextStepMs = nowMs + VISITOR_STEP_MS;
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
                visitor.nextStepMs = nowMs + VISITOR_STEP_MS;
                return;
            }

            if (visitor.actor.state == AdultState::CLIMB) {
                if (abs(visitor.actor.climbTargetX - visitor.x) <= 1) {
                    visitor.actor.state = AdultState::IDLE;
                    visitor.actor.stateTimer = 0;
                    scheduleVisitorWander(nowMs);
                    visitor.nextStepMs = nowMs + VISITOR_STEP_MS;
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
                visitor.nextStepMs = nowMs + VISITOR_STEP_MS;
                return;
            }

            if (abs(visitor.x - visitor.targetX) <= 1) {
                if (visitor.nextWanderMs == 0) {
                    visitor.actor.state = AdultState::IDLE;
                    scheduleVisitorWander(nowMs);
                    logVisitorState("arrived-idle", nowMs);
                }
                if (nowMs < visitor.nextWanderMs) {
                    visitor.nextStepMs = nowMs + VISITOR_STEP_MS;
                    return;
                }
                visitor.nextWanderMs = 0;
                chooseVisitorTarget();
            }
            if (visitor.x < visitor.targetX) {
                if (!visitor.faceRight) {
                    beginActorTurn(visitor.actor, true, true, nowMs, VISITOR_TURN_MS);
                    visitor.nextStepMs = nowMs + VISITOR_STEP_MS;
                    return;
                }
                visitor.actor.state = AdultState::WALK;
                visitor.x++;
            } else if (visitor.x > visitor.targetX) {
                if (visitor.faceRight) {
                    beginActorTurn(visitor.actor, false, true, nowMs, VISITOR_TURN_MS);
                    visitor.nextStepMs = nowMs + VISITOR_STEP_MS;
                    return;
                }
                visitor.actor.state = AdultState::WALK;
                visitor.x--;
            }
            visitor.nextStepMs = nowMs + (nowMs < visitorIntentUntilMs ? VISITOR_INTENT_STEP_MS : VISITOR_STEP_MS);
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
    }

    visit_eat_result_t eatResult;
    if (link.takeReceivedVisitEatResult(eatResult)) {
        if (eatResult.success && eatResult.hunger_gain > 0) {
            GameEngine::ins().getBug().modHunger((int8_t)eatResult.hunger_gain);
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

    if (!visitPingInFlight &&
        nowMs - lastVisitPingMs >= VISIT_PING_INTERVAL_MS &&
        link.sendVisitPing()) {
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
                             GameEngine::ins().isVisitHost())) {
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
    if (pushDir == 0) pushDir = faceRight ? 1 : -1;
    if (random(100) >= toyInterestPercent(bug.getTemperament())) {
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

    adultState = AdultState::IDLE;
    stateTimer = 0;
    stateDuration = (toyChargeDurationMs / 50) + 8;
    walkTargetIsRest = false;
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
    toyNoCatchUntilMs = nowMs + TOY_NO_CATCH_AFTER_HIT_MS;
    toyCharging = false;
    toyChargeStartMs = 0;
}

void TerrariumScene::reactLocalToToyImpact(uint32_t nowMs, int pushDir) {
    Bug& bug = GameEngine::ins().getBug();
    if (bug.getStage() != Stage::ADULT || bug.isDead()) return;
    if (adultState == AdultState::REST || adultState == AdultState::EAT) return;

    faceRight = pushDir >= 0;
    turnTargetFaceRight = faceRight;
    pokeReactionStartMs = nowMs;
    pokeReactionWasPoked = true;
    pokeThreatenEndMs = nowMs + POKE_REACTION_MS;
    alertUntilMs = nowMs + random(ALERT_MIN_MS, ALERT_MAX_MS + 1);
    adultState = AdultState::IDLE;
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

    visitorToyCharging = false;
    visitorToyChargeStartMs = 0;
    visitor.faceRight = pushDir >= 0;
    beginActorWalkTo(visitor.actor, visitor.x);
    visitor.actor.state = AdultState::THREATEN;
    visitor.actor.stateTimer = 0;
    visitor.actor.stateDuration = POKE_REACTION_MS;
    visitor.actor.threatenStartMs = nowMs;
    visitor.actor.threatenEndMs = nowMs + POKE_REACTION_MS;
    visitorIntentUntilMs = nowMs + VISITOR_INTENT_MS;
    visitor.turning = false;
    visitor.nextWanderMs = visitor.actor.threatenEndMs;
    logVisitorState("toy-impact-threaten", nowMs);
}

void TerrariumScene::reboundToyFromEntryImpact(uint32_t nowMs, int pushDir,
                                               float beetleTop, const ToySpec& spec) {
    if (pushDir == 0) pushDir = 1;
    toyY = beetleTop - (float)spec.radius - 3.0f;
    if (toyY < TOY_TANK_TOP + spec.radius) toyY = (float)(TOY_TANK_TOP + spec.radius);
    if (toyX < TOY_TANK_LEFT + spec.radius) toyX = (float)(TOY_TANK_LEFT + spec.radius);
    if (toyX > TOY_TANK_RIGHT - spec.radius) toyX = (float)(TOY_TANK_RIGHT - spec.radius);

    float vx = fabsf(toyVx) * 0.58f;
    float minVx = spec.baseImpulse * 0.42f + (float)random(0, 36);
    if (vx < minVx) vx = minVx;
    toyVx = vx * 0.9f * (float)pushDir;
    float vy = fabsf(toyVy) * 0.70f;
    float minVy = spec.liftImpulse * 1.05f + (float)random(0, 46);
    if (vy < minVy) vy = minVy;
    toyVy = -vy * 0.9f;
    toySpin = (float)(pushDir * random(9, 17)) * 0.9f;
    toyNoCatchUntilMs = nowMs + TOY_NO_CATCH_AFTER_REBOUND_MS;
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
    visitor.actor.state = AdultState::ATTACK_UP;
    visitor.actor.stateTimer = 0;
    visitor.actor.stateDuration = (TOY_ATTACK_UP_MS / 50) + 4;
}

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
    toyThrowTargetY = GROUND_Y - (int)(42.0f * targetScale) +
                      random(-7, 8);
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
    toyThrowTargetY = GROUND_Y - (int)(40.0f * targetScale) +
                      random(-5, 7);
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

    int pushDir = toyX >= toyEntryTargetX ? 1 : -1;
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
        x = (int)(toyThrowFromX + (toyThrowTargetX - toyThrowFromX) * t +
                  toyThrowCurveX * arc);
        y = (int)(toyThrowFromY + (toyThrowTargetY - toyThrowFromY) * easeIn);
        radius += (int)(2.0f * (1.0f - t));
    } else {
        float depthEase = 1.0f - (1.0f - t) * (1.0f - t);
        x = (int)(toyThrowFromX + (toyThrowTargetX - toyThrowFromX) * t +
                  toyThrowCurveX * arc);
        y = (int)(toyThrowFromY + (toyThrowTargetY - toyThrowFromY) * t -
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

void TerrariumScene::updateLarvaState(Bug& bug, uint32_t nowMs) {
    bool stateDone = nowMs - larvaStateStartMs >= larvaStateDurationMs;

    if (larvaState == LarvaState::EAT) {
        bug.setSleeping(false);
        if (bug.eatSubstrate(GameEngine::ins().getGameNow())) {
            observedLarvaEatGameMs = bug.getLastEatTime();
        }
        if (stateDone && bug.getHunger() > Bug::LARVA_SUBSTRATE_EAT_HUNGER) {
            enterLarvaState(LarvaState::IDLE, nowMs);
        }
        return;
    }

    if (larvaState == LarvaState::SLEEP) {
        bug.setSleeping(true);
        if (stateDone) {
            enterLarvaState(LarvaState::IDLE, nowMs);
        }
        return;
    }

    bug.setSleeping(false);
    if (stateDone) {
        bool hungry = bug.getHunger() <= Bug::LARVA_SUBSTRATE_EAT_HUNGER;
        bool chooseEat = hungry || random(100) < 65;
        enterLarvaState(chooseEat ? LarvaState::EAT : LarvaState::SLEEP, nowMs);
    }
}

SceneID TerrariumScene::update() {
    animFrame++;

    GameEngine& engine = GameEngine::ins();
    Bug& bug = GameEngine::ins().getBug();
    uint32_t nowMs = Hal::ins().millis();
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
    } else if (bug.getStage() == Stage::LARVA && !bug.isDead()) {
        updateLarvaState(bug, nowMs);
    } else {
        larvaState = LarvaState::IDLE;
        observedLarvaEatGameMs = bug.getLastEatTime();
        bug.setSleeping(false);
    }

    // 更新甲虫心智（成虫期且存活）
    if (bug.getStage() == Stage::ADULT && !bug.isDead()) {
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
    } else if (bug.getStage() != Stage::ADULT) {
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

    drawBackground();
    drawFoodTray();
    drawWood();
    if (GameEngine::ins().isVisitGuest()) {
        drawVisitAwayOverlay();
        if (visitRecallConfirm) {
            drawVisitRecallConfirm();
        }
        return;
    }

    drawBug();
    drawVisitor();
    if (isMobileBeetleStage(bug.getStage()) && !bug.isDead()) {
        drawToy();
    }
    drawToyEntry();
    drawPokeAction();
    drawStatusBar();

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
            // 卵期短按 A：喷水
            bug.onEggWater(gameNow);
            Serial.println("[Terrarium] Watered egg");
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

void TerrariumScene::drawBackground() {
    if (GameEngine::ins().getMainSceneBg() == GameEngine::BG_BEGINNER) {
        PixelRenderer::drawIndexed8(0, 0,
                                    MainSceneAssets::GIRL_ROOM_FULL_W,
                                    MainSceneAssets::GIRL_ROOM_FULL_H,
                                    MainSceneAssets::GIRL_ROOM_FULL_INDEX,
                                    MainSceneAssets::GIRL_ROOM_FULL_PALETTE);
        return;
    }
    if (GameEngine::ins().getMainSceneBg() == GameEngine::BG_CHILD_ROOM) {
        PixelRenderer::drawIndexed8(0, 0,
                                    MainSceneAssets::BOY_ROOM_FULL_W,
                                    MainSceneAssets::BOY_ROOM_FULL_H,
                                    MainSceneAssets::BOY_ROOM_FULL_INDEX,
                                    MainSceneAssets::BOY_ROOM_FULL_PALETTE);
        return;
    }
    if (GameEngine::ins().getMainSceneBg() == GameEngine::BG_ENTOMOLOGIST) {
        if (GameEngine::ins().isNight()) {
            PixelRenderer::drawIndexed8(0, 0,
                                        MainSceneAssets::ENTOMOLOGIST_ROOM_NIGHT_FULL_W,
                                        MainSceneAssets::ENTOMOLOGIST_ROOM_NIGHT_FULL_H,
                                        MainSceneAssets::ENTOMOLOGIST_ROOM_NIGHT_FULL_INDEX,
                                        MainSceneAssets::ENTOMOLOGIST_ROOM_NIGHT_FULL_PALETTE);
        } else {
            PixelRenderer::drawIndexed8(0, 0,
                                        MainSceneAssets::ENTOMOLOGIST_ROOM_DAY_FULL_W,
                                        MainSceneAssets::ENTOMOLOGIST_ROOM_DAY_FULL_H,
                                        MainSceneAssets::ENTOMOLOGIST_ROOM_DAY_FULL_INDEX,
                                        MainSceneAssets::ENTOMOLOGIST_ROOM_DAY_FULL_PALETTE);
        }
        return;
    }
    if (GameEngine::ins().getMainSceneBg() == GameEngine::BG_SCHOOL) {
        if (GameEngine::ins().isNight()) {
            PixelRenderer::drawIndexed8(0, 0,
                                        MainSceneAssets::SCHOOL_NIGHT_FULL_W,
                                        MainSceneAssets::SCHOOL_NIGHT_FULL_H,
                                        MainSceneAssets::SCHOOL_NIGHT_FULL_INDEX,
                                        MainSceneAssets::SCHOOL_NIGHT_FULL_PALETTE);
        } else {
            PixelRenderer::drawIndexed8(0, 0,
                                        MainSceneAssets::SCHOOL_DAY_FULL_W,
                                        MainSceneAssets::SCHOOL_DAY_FULL_H,
                                        MainSceneAssets::SCHOOL_DAY_FULL_INDEX,
                                        MainSceneAssets::SCHOOL_DAY_FULL_PALETTE);
        }
        return;
    }

    PixelRenderer::drawIndexed8(0, 0,
                                MainSceneAssets::ROOM_FULL_W,
                                MainSceneAssets::ROOM_FULL_H,
                                MainSceneAssets::ROOM_FULL_INDEX,
                                MainSceneAssets::ROOM_FULL_PALETTE);
}

void TerrariumScene::drawBug() {
    Bug& bug = GameEngine::ins().getBug();
    uint8_t pal = bug.getPaletteId();

    // 戳反应覆盖绘制
    if (pokeReactionEndMs != 0 && Hal::ins().millis() < pokeReactionEndMs) {
        if (pokeReactionWasPoked) {
            if (bug.getStage() == Stage::EGG) {
                int shake = ((Hal::ins().millis() / 50) % 2) ? 2 : -2;
                drawEgg(bugX + shake, GROUND_Y - 7, pal);
                return;
            }
            if (bug.getStage() == Stage::LARVA) {
                drawLarvaPoked(bugX, GROUND_Y - 5, pal);
                return;
            }
            if (bug.getStage() == Stage::PUPA) {
                int shake = ((Hal::ins().millis() / 100) % 2) ? PUPA_POKE_SHAKE_PX : -PUPA_POKE_SHAKE_PX;
                drawPupa(Hal::DISPLAY_W / 2 + shake,
                         Hal::DISPLAY_H - PUPA_BOTTOM_MARGIN_PX - HerculesPupaSprites::FRAME_H / 2,
                         pal);
                return;
            }
            // 成虫：fall through 到 drawAdult，其内部会处理 poke 反应（威吓姿态）
        } else {
            // 冷却中：在甲虫上方画闪烁提示
            int baseY = 95;
            if (bug.getStage() == Stage::EGG) baseY = GROUND_Y - 7;
            else if (bug.getStage() == Stage::LARVA) baseY = GROUND_Y - 5;
            else if (bug.getStage() == Stage::ADULT) baseY = GROUND_Y;
            drawPokeCooldownHint(bugX, baseY);
        }
    }

    switch (bug.getStage()) {
        case Stage::EGG:
            drawEgg(bugX, GROUND_Y - 7, pal);
            break;
        case Stage::LARVA:
            drawLarva(bugX, GROUND_Y - 5, pal);
            break;
        case Stage::PUPA:
            drawPupa(Hal::DISPLAY_W / 2,
                     Hal::DISPLAY_H - PUPA_BOTTOM_MARGIN_PX - HerculesPupaSprites::FRAME_H / 2,
                     pal);
            break;
        case Stage::JUVENILE:
            drawAdult(bugX, GROUND_Y, pal);
            break;
        case Stage::ADULT:
        default:
            drawAdult(bugX, GROUND_Y, pal);
            break;
    }
}

float TerrariumScene::visitorAdultScale() const {
    if (visitor.siz < 6) return 0.9f;
    if (visitor.siz < 14) return 1.0f;
    if (visitor.siz < 22) return 1.1f;
    return 1.2f;
}

float TerrariumScene::visitorAdultDrawScale() const {
    float scale = visitorAdultScale();
    return scale < 1.0f ? 1.0f : scale;
}

uint16_t TerrariumScene::visitorAdultColor() const {
    if ((visitor.palette & 0x80) != 0) {
        uint8_t temper = visitor.palette & 0x07;
        if (temper > (uint8_t)Temperament::SPIRIT) temper = (uint8_t)Temperament::BRUTE;
        uint8_t bucket = (visitor.palette >> 3) & 0x03;
        float depth = (float)bucket / 3.0f;
        return adultDepthColor((Temperament)temper, depth);
    }

    switch (visitor.palette & 0x03) {
        case 1: return 0x0400;
        case 2: return 0xFE00;
        case 3: return 0xE71C;
        case 0:
        default:
            return adultHueMain(Temperament::BRUTE);
    }
}

void TerrariumScene::drawVisitorAdult(int x, int y) {
    const HerculesAdultSprites::RleFrame* frames = HerculesAdultSprites::WALK_FRAMES;
    const uint16_t* data = HerculesAdultSprites::WALK_RLE;
    uint8_t frameCount = HerculesAdultSprites::WALK_FRAME_COUNT;
    uint8_t frameIndex = visitor.falling ? 0 : (animFrame / 14) % frameCount;
    bool flipSprite = !visitor.faceRight;

    if (visitor.actor.state == AdultState::THREATEN) {
        frames = HerculesAdultSprites::THREATEN_FRAMES;
        data = HerculesAdultSprites::THREATEN_RLE;
        frameCount = HerculesAdultSprites::THREATEN_FRAME_COUNT;
        uint32_t elapsed = Hal::ins().millis() - visitor.actor.threatenStartMs;
        if (elapsed < THREATEN_PLAY_MS) {
            frameIndex = (elapsed * frameCount) / THREATEN_PLAY_MS;
            if (frameIndex >= frameCount) frameIndex = frameCount - 1;
        } else if (elapsed < THREATEN_PLAY_MS + THREATEN_HOLD_MS) {
            frameIndex = frameCount - 1;
        } else {
            uint32_t returnElapsed = elapsed - THREATEN_PLAY_MS - THREATEN_HOLD_MS;
            uint8_t reverseStep = (returnElapsed * frameCount) / THREATEN_RETURN_MS;
            frameIndex = reverseStep >= frameCount ? 0 : (frameCount - 1 - reverseStep);
        }
        flipSprite = !visitor.faceRight;
    } else if (visitor.actor.state == AdultState::ATTACK_DOWN) {
        frames = HerculesAdultSprites::ATTACK_DOWN_FRAMES;
        data = HerculesAdultSprites::ATTACK_DOWN_RLE;
        frameCount = HerculesAdultSprites::ATTACK_DOWN_FRAME_COUNT;
        uint32_t elapsed = Hal::ins().millis() - visitorToyChargeStartMs;
        uint32_t duration = visitorToyChargeDurationMs == 0 ? 1 : visitorToyChargeDurationMs;
        frameIndex = (elapsed * frameCount) / duration;
        if (frameIndex >= frameCount) frameIndex = frameCount - 1;
        flipSprite = !visitor.faceRight;
    } else if (visitor.actor.state == AdultState::ATTACK_UP) {
        uint32_t elapsed = Hal::ins().millis() - visitorToyAttackStartMs;
        frames = HerculesAdultSprites::ATTACK_UP_FRAMES;
        data = HerculesAdultSprites::ATTACK_UP_RLE;
        frameCount = HerculesAdultSprites::ATTACK_UP_FRAME_COUNT;
        frameIndex = (elapsed * frameCount) / TOY_ATTACK_UP_MS;
        if (frameIndex >= frameCount) frameIndex = frameCount - 1;
        flipSprite = !visitor.faceRight;
    } else if (visitor.actor.state == AdultState::EAT) {
        frames = HerculesAdultSprites::EAT_FRAMES;
        data = HerculesAdultSprites::EAT_RLE;
        frameCount = HerculesAdultSprites::EAT_FRAME_COUNT;
        frameIndex = (visitor.actor.stateTimer / EAT_FRAME_INTERVAL_MIN) % frameCount;
        flipSprite = !visitor.faceRight;
    } else if (visitor.actor.state == AdultState::TURN) {
        frames = HerculesAdultSprites::TURN_FRAMES;
        data = HerculesAdultSprites::TURN_RLE;
        frameCount = HerculesAdultSprites::TURN_FRAME_COUNT;
        frameIndex = visitor.actor.turnFrameIndex;
        if (frameIndex >= frameCount) frameIndex = frameCount - 1;
        flipSprite = false;
    } else if (visitor.actor.state == AdultState::REST) {
        uint32_t getDownFrames = HerculesAdultSprites::SLEEP_GETDOWN_FRAME_COUNT *
                                 REST_GETDOWN_FRAME_INTERVAL;
        if (visitor.actor.stateTimer < getDownFrames) {
            frames = HerculesAdultSprites::SLEEP_GETDOWN_FRAMES;
            data = HerculesAdultSprites::SLEEP_GETDOWN_RLE;
            frameCount = HerculesAdultSprites::SLEEP_GETDOWN_FRAME_COUNT;
            frameIndex = visitor.actor.stateTimer / REST_GETDOWN_FRAME_INTERVAL;
            if (frameIndex >= frameCount) frameIndex = frameCount - 1;
        } else {
            frames = HerculesAdultSprites::SLEEP_BREATH_FRAMES;
            data = HerculesAdultSprites::SLEEP_BREATH_RLE;
            frameCount = HerculesAdultSprites::SLEEP_BREATH_FRAME_COUNT;
            frameIndex = ((visitor.actor.stateTimer - getDownFrames) / REST_BREATH_FRAME_INTERVAL) % frameCount;
        }
        flipSprite = !visitor.faceRight;
    } else if (visitor.actor.state == AdultState::SLIDE) {
        frameIndex = 0;
        flipSprite = !visitor.faceRight;
    } else if (visitor.actor.state == AdultState::CLIMB) {
        frameIndex = (animFrame / 20) % frameCount;
        flipSprite = !visitor.faceRight;
    } else if (visitor.actor.state == AdultState::IDLE) {
        frameIndex = 0;
    }

    uint16_t offset = pgm_read_word(&frames[frameIndex].offset);
    uint16_t length = pgm_read_word(&frames[frameIndex].length);
    uint8_t frameW = pgm_read_byte(&frames[frameIndex].width);
    uint8_t frameH = pgm_read_byte(&frames[frameIndex].height);
    float scale = visitorAdultDrawScale();
    uint16_t color = visitorAdultColor();
    int drawX = (int)(x - (frameW * scale) / 2.0f);
    int drawY = (int)(y - frameH * scale);
    if (visitor.faceRight && visitor.actor.state != AdultState::TURN) {
        drawX -= 2;
    }

    PixelRenderer::drawRgb565RleMappedScaled(drawX,
                                             drawY,
                                             frameW,
                                             frameH,
                                             data, offset, length,
                                             scale,
                                             HerculesAdultSprites::PALETTE_KEY, color,
                                             HerculesAdultSprites::PALETTE_KEY, color,
                                             HerculesAdultSprites::PALETTE_KEY, color,
                                             flipSprite);
}

void TerrariumScene::drawVisitor() {
    if (!visitor.active) return;
    int drawY = visitor.y;
    drawVisitorAdult(visitor.x, drawY);
}

void TerrariumScene::drawEgg(int x, int y, uint8_t palette) {
    (void)palette;
    Bug& bug = GameEngine::ins().getBug();
    float progress = bug.getStageProgress(GameEngine::ins().getGameNow());
    uint8_t frameIndex = (uint8_t)(progress * HerculesEggSprites::FRAME_COUNT);
    if (frameIndex >= HerculesEggSprites::FRAME_COUNT) {
        frameIndex = HerculesEggSprites::FRAME_COUNT - 1;
    }

    uint16_t offset = pgm_read_word(&HerculesEggSprites::FRAMES[frameIndex].offset);
    uint16_t length = pgm_read_word(&HerculesEggSprites::FRAMES[frameIndex].length);
    uint8_t w = pgm_read_byte(&HerculesEggSprites::FRAMES[frameIndex].width);
    uint8_t h = pgm_read_byte(&HerculesEggSprites::FRAMES[frameIndex].height);
    PixelRenderer::drawRgb565Rle(x - w / 2, y + 9 - h, w, h,
                                 HerculesEggSprites::RLE, offset, length);
}

void TerrariumScene::drawLarva(int x, int y, uint8_t palette) {
    (void)palette;
    Bug& bug = GameEngine::ins().getBug();
    float progress = bug.getStageProgress(GameEngine::ins().getGameNow());
    uint8_t ageIndex = (uint8_t)(progress * HerculesLarvaSprites::AGE_COUNT);
    if (ageIndex >= HerculesLarvaSprites::AGE_COUNT) {
        ageIndex = HerculesLarvaSprites::AGE_COUNT - 1;
    }

    bool sleepingVisual = larvaState == LarvaState::SLEEP;
    const auto* frames = HerculesLarvaSprites::IDLE_FRAMES;
    const uint16_t* rle = HerculesLarvaSprites::IDLE_RLE;
    uint8_t frameIndex = ageIndex;
    bool eatingVisual = larvaState == LarvaState::EAT;
    if (eatingVisual) {
        uint32_t elapsed = Hal::ins().millis() - larvaStateStartMs;
        uint8_t eatFrame = (elapsed / LARVA_EAT_FRAME_MS) % HerculesLarvaSprites::EAT_FRAME_COUNT;
        frameIndex = ageIndex * HerculesLarvaSprites::EAT_FRAME_COUNT + eatFrame;
        frames = HerculesLarvaSprites::EAT_FRAMES;
        rle = HerculesLarvaSprites::EAT_RLE;
    } else if (sleepingVisual) {
        static constexpr uint8_t LARVA_SLEEP_BREATH_SEQUENCE[] = {0, 1, 2, 1, 0};
        uint8_t sequenceIndex = (Hal::ins().millis() / 650) %
                                (sizeof(LARVA_SLEEP_BREATH_SEQUENCE) /
                                 sizeof(LARVA_SLEEP_BREATH_SEQUENCE[0]));
        uint8_t sleepFrame = LARVA_SLEEP_BREATH_SEQUENCE[sequenceIndex];
        frameIndex = ageIndex * HerculesLarvaSprites::SLEEP_FRAME_COUNT + sleepFrame;
        frames = HerculesLarvaSprites::SLEEP_FRAMES;
        rle = HerculesLarvaSprites::SLEEP_RLE;
    }

    uint16_t offset = pgm_read_word(&frames[frameIndex].offset);
    uint16_t length = pgm_read_word(&frames[frameIndex].length);
    uint8_t w = pgm_read_byte(&frames[frameIndex].width);
    uint8_t h = pgm_read_byte(&frames[frameIndex].height);
    PixelRenderer::drawRgb565Rle(x - w / 2, y + 5 - h, w, h, rle, offset, length);
}

void TerrariumScene::drawPupa(int x, int y, uint8_t palette) {
    (void)palette;
    Bug& bug = GameEngine::ins().getBug();
    float progress = bug.getStageProgress(GameEngine::ins().getGameNow());
    uint8_t frameIndex = (uint8_t)(progress * HerculesPupaSprites::FRAME_COUNT);
    if (frameIndex >= HerculesPupaSprites::FRAME_COUNT) {
        frameIndex = HerculesPupaSprites::FRAME_COUNT - 1;
    }

    uint16_t offset = pgm_read_word(&HerculesPupaSprites::FRAMES[frameIndex].offset);
    uint16_t length = pgm_read_word(&HerculesPupaSprites::FRAMES[frameIndex].length);
    uint8_t w = pgm_read_byte(&HerculesPupaSprites::FRAMES[frameIndex].width);
    uint8_t h = pgm_read_byte(&HerculesPupaSprites::FRAMES[frameIndex].height);
    PixelRenderer::drawRgb565Rle(x - w / 2, y - h / 2, w, h,
                                 HerculesPupaSprites::RLE, offset, length);
}

void TerrariumScene::drawAdult(int x, int y, uint8_t palette) {
    (void)palette;

    Bug& bug = GameEngine::ins().getBug();
    const HerculesAdultSprites::RleFrame* frames = HerculesAdultSprites::WALK_FRAMES;
    const uint16_t* data = HerculesAdultSprites::WALK_RLE;
    uint8_t frameCount = HerculesAdultSprites::WALK_FRAME_COUNT;
    uint8_t frameIndex = (animFrame / 10) % frameCount;
    bool flipSprite = !faceRight;

    if (bug.isDead()) {
        // 死亡叠层：使用 reset 第 0 帧
        frames = HerculesAdultSprites::RESET_FRAMES;
        data = HerculesAdultSprites::RESET_RLE;
        frameCount = HerculesAdultSprites::RESET_FRAME_COUNT;
        frameIndex = 0;
        flipSprite = !faceRight;
    } else if (pokeThreatenEndMs != 0 && pokeReactionWasPoked && bug.getStage() == Stage::ADULT) {
        // 戳反应：威吓正播、保持，结束前反向播放回站姿，避免直接跳回 walk。
        frames = HerculesAdultSprites::THREATEN_FRAMES;
        data = HerculesAdultSprites::THREATEN_RLE;
        frameCount = HerculesAdultSprites::THREATEN_FRAME_COUNT;
        uint32_t elapsed = Hal::ins().millis() - pokeReactionStartMs;
        if (elapsed < THREATEN_PLAY_MS) {
            frameIndex = (elapsed * frameCount) / THREATEN_PLAY_MS;
            if (frameIndex >= frameCount) frameIndex = frameCount - 1;
        } else if (elapsed < THREATEN_PLAY_MS + THREATEN_HOLD_MS) {
            frameIndex = frameCount - 1;
        } else {
            uint32_t returnElapsed = elapsed - THREATEN_PLAY_MS - THREATEN_HOLD_MS;
            uint8_t reverseStep = (returnElapsed * frameCount) / THREATEN_RETURN_MS;
            frameIndex = reverseStep >= frameCount ? 0 : (frameCount - 1 - reverseStep);
        }
        flipSprite = !faceRight;
    } else if (toyCharging && isMobileBeetleStage(bug.getStage())) {
        frames = HerculesAdultSprites::ATTACK_DOWN_FRAMES;
        data = HerculesAdultSprites::ATTACK_DOWN_RLE;
        frameCount = HerculesAdultSprites::ATTACK_DOWN_FRAME_COUNT;
        uint32_t elapsed = Hal::ins().millis() - toyChargeStartMs;
        uint32_t duration = toyChargeDurationMs == 0 ? 1 : toyChargeDurationMs;
        frameIndex = (elapsed * frameCount) / duration;
        if (frameIndex >= frameCount) frameIndex = frameCount - 1;
        flipSprite = !faceRight;
    } else if (toyAttackStartMs != 0 &&
               Hal::ins().millis() - toyAttackStartMs < TOY_ATTACK_UP_MS &&
               isMobileBeetleStage(bug.getStage())) {
        uint32_t elapsed = Hal::ins().millis() - toyAttackStartMs;
        frames = HerculesAdultSprites::ATTACK_UP_FRAMES;
        data = HerculesAdultSprites::ATTACK_UP_RLE;
        frameCount = HerculesAdultSprites::ATTACK_UP_FRAME_COUNT;
        frameIndex = (elapsed * frameCount) / TOY_ATTACK_UP_MS;
        if (frameIndex >= frameCount) frameIndex = frameCount - 1;
        flipSprite = !faceRight;
    } else if (adultState == AdultState::REST) {
        // 夜间休息：先播放一次入睡 getDown，之后只循环 breath。
        uint32_t getDownFrames = HerculesAdultSprites::SLEEP_GETDOWN_FRAME_COUNT *
                                 REST_GETDOWN_FRAME_INTERVAL;
        if (stateTimer < getDownFrames) {
            frames = HerculesAdultSprites::SLEEP_GETDOWN_FRAMES;
            data = HerculesAdultSprites::SLEEP_GETDOWN_RLE;
            frameCount = HerculesAdultSprites::SLEEP_GETDOWN_FRAME_COUNT;
            frameIndex = stateTimer / REST_GETDOWN_FRAME_INTERVAL;
            if (frameIndex >= frameCount) frameIndex = frameCount - 1;
        } else {
            frames = HerculesAdultSprites::SLEEP_BREATH_FRAMES;
            data = HerculesAdultSprites::SLEEP_BREATH_RLE;
            frameCount = HerculesAdultSprites::SLEEP_BREATH_FRAME_COUNT;
            frameIndex = ((stateTimer - getDownFrames) / REST_BREATH_FRAME_INTERVAL) % frameCount;
        }
        flipSprite = !faceRight;
    } else if (adultState == AdultState::EAT) {
        // 进食动画
        frames = HerculesAdultSprites::EAT_FRAMES;
        data = HerculesAdultSprites::EAT_RLE;
        frameCount = HerculesAdultSprites::EAT_FRAME_COUNT;
        uint8_t interval = eatFrameInterval == 0 ? EAT_FRAME_INTERVAL_MIN : eatFrameInterval;
        frameIndex = (stateTimer / interval) % frameCount;
        flipSprite = !faceRight;
    } else if (adultState == AdultState::TURN) {
        // 转身过渡每次随机取一张中间姿态；同一次转身中保持不变。
        frames = HerculesAdultSprites::TURN_FRAMES;
        data = HerculesAdultSprites::TURN_RLE;
        frameCount = HerculesAdultSprites::TURN_FRAME_COUNT;
        frameIndex = turnFrameIndex;
        if (frameIndex >= frameCount) frameIndex = frameCount - 1;
        flipSprite = false;
    } else if (adultState == AdultState::SLIDE) {
        // 滑落：保持当前姿势滑下去，不播放 walk 动画
        frameIndex = 0;
        flipSprite = !faceRight;
    } else if (adultState == AdultState::CLIMB) {
        // 爬坡：使用 walk 帧，很慢播放
        frameIndex = (animFrame / 20) % frameCount;
        flipSprite = !faceRight;
    } else if (adultState == AdultState::IDLE) {
        // 静止时停在 walk 第 0 帧（站立姿态）
        frameIndex = 0;
        flipSprite = !faceRight;
    }

    uint16_t offset = pgm_read_word(&frames[frameIndex].offset);
    uint16_t length = pgm_read_word(&frames[frameIndex].length);
    uint8_t frameW = pgm_read_byte(&frames[frameIndex].width);
    uint8_t frameH = pgm_read_byte(&frames[frameIndex].height);
    float adultScale = bug.getAdultScale();
    uint16_t adultHue = adultDepthColor(bug.getTemperament(), bug.getAdultDepth());
    int drawX = (int)(x - (frameW * adultScale) / 2.0f);
    int drawY = (int)(y - frameH * adultScale);
    // y 是脚/底部参考点，让精灵底部对齐 y
    PixelRenderer::drawRgb565RleMappedScaled(drawX,
                                             drawY,
                                             frameW,
                                             frameH,
                                             data, offset, length,
                                             adultScale,
                                             HerculesAdultSprites::PALETTE_KEY, adultHue,
                                             HerculesAdultSprites::PALETTE_KEY, adultHue,
                                             HerculesAdultSprites::PALETTE_KEY, adultHue,
                                             flipSprite);
}

// 判断成虫是否想去进食：饥饿或纯粹嘴馋
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
            // 静止后段偶尔张望。这里是每帧概率，20fps 下不能设得太高。
            if (stateTimer > (stateDuration * 2) / 3 &&
                random(1000) < IDLE_LOOK_AROUND_CHANCE_PER_1000) {
                startTurn(!faceRight, false);
                break;
            }
            // 静止结束后由 AI 心智决定下一步
            if (stateTimer >= stateDuration) {
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
                if (!GameEngine::ins().isNight() && stateTimer > 180 && random(1000) < 4) {
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

    switch (adultState) {
        case AdultState::IDLE:
            if (stateTimer > (stateDuration / 2) && random(1000) < 12) {
                startTurn(!faceRight, false);
                break;
            }
            if (stateTimer >= stateDuration) {
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

void TerrariumScene::drawStatusBar() {
    Bug& bug = GameEngine::ins().getBug();
    if (GameEngine::ins().isVisitGuest()) return;

    LGFX_Sprite& canvas = Hal::ins().canvas();
    float fs = PixelRenderer::getContentFontScale();

    static constexpr int HUD_Y = 2;
    int textH = (int)(8 * fs + 0.5f);

    auto drawIcon = [&](int x, int y, const uint16_t* rows, int w, int h, uint16_t color) {
        for (int row = 0; row < h; row++) {
            uint16_t mask = rows[row];
            for (int col = 0; col < w; col++) {
                if (mask & (1 << (w - 1 - col))) {
                    PixelRenderer::fillRect(x + col, y + row, 1, 1, color);
                }
            }
        }
    };

    // ---- 极简 HUD：时间浮在右上角，状态压缩在下方 ----
    char clockBuf[8];
    GameEngine::ins().getExploreClockText(clockBuf, sizeof(clockBuf));
    PixelRenderer::applyTextStyle(fs);
    int timeW = canvas.textWidth(clockBuf);
    int timeX = Hal::DISPLAY_W - timeW - 4;
    int timeY = HUD_Y;

    uint8_t mot = bug.getMot();
    static const uint16_t HEART_MASK[9] = {
        0b000000000,
        0b011000110,
        0b111101111,
        0b111111111,
        0b111111111,
        0b011111110,
        0b001111100,
        0b000111000,
        0b000010000,
    };
    static constexpr int ICON_SIZE = 9;
    static constexpr int ICON_GAP = 4;
    int iconY = HUD_Y + textH + 4;
    int weatherX = Hal::DISPLAY_W - ICON_SIZE - 5;
    int heartX = weatherX - ICON_GAP - ICON_SIZE;
    int totalRows = ICON_SIZE;

    auto drawHeartMeter = [&](int x, int y, uint8_t value) {
        int rows = (value * totalRows + 50) / 100;
        if (rows < 1 && value > 0) rows = 1;
        if (rows > totalRows) rows = totalRows;
        for (int row = 0; row < totalRows; row++) {
            uint16_t mask = HEART_MASK[row];
            bool fill = (row >= totalRows - rows);
            uint16_t c = fill ? PixelRenderer::RED : PixelRenderer::GRAY;
            for (int col = 0; col < ICON_SIZE; col++) {
                if (mask & (1 << (ICON_SIZE - 1 - col))) {
                    PixelRenderer::fillRect(x + col, y + row, 1, 1, c);
                }
            }
        }
    };
    auto hungerColor = [](uint8_t hunger) {
        return hunger > 50 ? PixelRenderer::GREEN :
               (hunger > 20 ? PixelRenderer::YELLOW : PixelRenderer::RED);
    };
    auto drawHourglassMeter = [&](int x, int y, uint32_t remaining, uint32_t duration) {
        static const uint16_t HOURGLASS_MASK[9] = {
            0b111111111,
            0b011111110,
            0b001111100,
            0b000111000,
            0b000010000,
            0b000111000,
            0b001111100,
            0b011111110,
            0b111111111,
        };
        int rows = duration == 0 ? 0 : (int)(((uint64_t)remaining * totalRows + duration / 2) / duration);
        if (rows < 1 && remaining > 0) rows = 1;
        if (rows > totalRows) rows = totalRows;
        for (int row = 0; row < totalRows; row++) {
            uint16_t mask = HOURGLASS_MASK[row];
            bool fill = (row >= totalRows - rows);
            uint16_t c = fill ? PixelRenderer::YELLOW : PixelRenderer::GRAY;
            for (int col = 0; col < ICON_SIZE; col++) {
                if (mask & (1 << (ICON_SIZE - 1 - col))) {
                    PixelRenderer::fillRect(x + col, y + row, 1, 1, c);
                }
            }
        }
    };

    if (GameEngine::ins().isVisitHost()) {
        const VisitBugSnapshot& guest = GameEngine::ins().getVisitRemoteBug();
        static const uint16_t WEATHER_SUN_VISIT[9] = {
            0b100010001,
            0b010010010,
            0b001111100,
            0b011111110,
            0b111111111,
            0b011111110,
            0b001111100,
            0b010010010,
            0b100010001,
        };
        static const uint16_t WEATHER_CLOUD_VISIT[9] = {
            0b000000000,
            0b000111000,
            0b001111100,
            0b011111110,
            0b111111111,
            0b111111111,
            0b011111110,
            0b000000000,
            0b000000000,
        };
        int statusW = (weatherX + ICON_SIZE) - heartX;
        int hostHeartX = heartX;
        int guestHeartX = hostHeartX - ICON_GAP - statusW;
        int guestBarX = guestHeartX;
        int guestHourglassX = guestBarX + statusW - ICON_SIZE;
        if (guestHeartX < 0) {
            guestHeartX = 0;
            guestBarX = 0;
            guestHourglassX = statusW - ICON_SIZE;
        }
        int barY = iconY + ICON_SIZE + 3;
        int barH = 4;
        int panelX = (timeX < guestBarX ? timeX : guestBarX) - 4;
        if (panelX < 0) panelX = 0;
        int panelW = Hal::DISPLAY_W - panelX;
        int panelH = barY + barH + 3;
        for (int y = 0; y < panelH; y++) {
            for (int x = panelX; x < panelX + panelW; x++) {
                if (((x + y) & 1) == 0) {
                    PixelRenderer::fillRect(x, y, 1, 1, PixelRenderer::BLACK);
                }
            }
        }

        PixelRenderer::drawPixelText(timeX + 1, timeY + 1, clockBuf, PixelRenderer::BLACK, fs);
        PixelRenderer::drawPixelText(timeX, timeY, clockBuf, PixelRenderer::WHITE, fs);
        drawHeartMeter(guestHeartX, iconY, guest.motivation);
        drawHourglassMeter(guestHourglassX, iconY,
                           GameEngine::ins().getVisitRemainingMs(),
                           GameEngine::ins().getVisitDurationMs());
        drawHeartMeter(hostHeartX, iconY, bug.getMot());
        uint32_t gameDay = GameEngine::ins().getCurrentGameDay();
        bool cloudy = (gameDay % 3 == 0);
        drawIcon(weatherX, iconY,
                 cloudy ? WEATHER_CLOUD_VISIT : WEATHER_SUN_VISIT,
                 ICON_SIZE, ICON_SIZE,
                 cloudy ? PixelRenderer::WHITE : PixelRenderer::YELLOW);
        PixelRenderer::drawProgressBar(guestBarX, barY, statusW, barH,
                                       guest.hunger / 100.0f,
                                       hungerColor(guest.hunger),
                                       PixelRenderer::GRAY);
        PixelRenderer::drawProgressBar(heartX, barY, statusW, barH,
                                       bug.getHunger() / 100.0f,
                                       hungerColor(bug.getHunger()),
                                       PixelRenderer::GRAY);
        return;
    }

    int fillRows = (mot * totalRows + 50) / 100;
    if (fillRows < 1 && mot > 0) fillRows = 1;
    if (fillRows > totalRows) fillRows = totalRows;

    int barY = iconY + ICON_SIZE + 3;
    int barH = 4;
    int panelX = (timeX < heartX ? timeX : heartX) - 4;
    if (panelX < 0) panelX = 0;
    int panelY = 0;
    int panelW = Hal::DISPLAY_W - panelX;
    int panelH = barY + barH + 3;
    for (int y = panelY; y < panelY + panelH; y++) {
        for (int x = panelX; x < panelX + panelW; x++) {
            if (((x + y) & 1) == 0) {
                PixelRenderer::fillRect(x, y, 1, 1, PixelRenderer::BLACK);
            }
        }
    }

    PixelRenderer::drawPixelText(timeX + 1, timeY + 1, clockBuf, PixelRenderer::BLACK, fs);
    PixelRenderer::drawPixelText(timeX, timeY, clockBuf, PixelRenderer::WHITE, fs);

    for (int row = 0; row < totalRows; row++) {
        uint16_t mask = HEART_MASK[row];
        bool fill = (row >= totalRows - fillRows);
        uint16_t c = fill ? PixelRenderer::RED : PixelRenderer::GRAY;
        for (int col = 0; col < ICON_SIZE; col++) {
            if (mask & (1 << (ICON_SIZE - 1 - col))) {
                PixelRenderer::fillRect(heartX + col, iconY + row, 1, 1, c);
            }
        }
    }

    static const uint16_t WEATHER_SUN[9] = {
        0b100010001,
        0b010010010,
        0b001111100,
        0b011111110,
        0b111111111,
        0b011111110,
        0b001111100,
        0b010010010,
        0b100010001,
    };
    static const uint16_t WEATHER_CLOUD[9] = {
        0b000000000,
        0b000111000,
        0b001111100,
        0b011111110,
        0b111111111,
        0b111111111,
        0b011111110,
        0b000000000,
        0b000000000,
    };
    uint32_t gameDay = GameEngine::ins().getCurrentGameDay();
    bool cloudy = (gameDay % 3 == 0);
    const uint16_t* icon = cloudy ? WEATHER_CLOUD : WEATHER_SUN;
    uint16_t iconColor = cloudy ? PixelRenderer::WHITE : PixelRenderer::YELLOW;
    drawIcon(weatherX, iconY, icon, ICON_SIZE, ICON_SIZE, iconColor);

    uint16_t hColor = hungerColor(bug.getHunger());
    int barX = heartX;
    int barW = (weatherX + ICON_SIZE) - heartX;
    PixelRenderer::drawProgressBar(barX, barY, barW, barH, bug.getHunger() / 100.0f, hColor, PixelRenderer::GRAY);
}

void TerrariumScene::drawVisitAwayOverlay() {
    LGFX_Sprite& canvas = Hal::ins().canvas();
    uint32_t remainingMs = GameEngine::ins().getVisitRemainingMs();
    uint32_t minutes = (remainingMs + 59999UL) / 60000UL;
    if (minutes < 1 && remainingMs > 0) minutes = 1;

    char line2[24];
    snprintf(line2, sizeof(line2), UiStrings::VISIT_LEFT_FMT, (unsigned)minutes);
    const char* line3 = UiStrings::VISIT_RECALL_HINT;

    float titleFs = 2.0f;
    float bodyFs = PixelRenderer::getContentFontScale();
    int titleY = 56;
    int bodyY = 78;
    int hintY = 98;

    PixelRenderer::applyTextStyle(titleFs);
    int titleW = canvas.textWidth(UiStrings::VISIT_AWAY_TITLE);
    PixelRenderer::applyTextStyle(bodyFs);
    int bodyW = canvas.textWidth(line2);
    PixelRenderer::applyTextStyle(1.0f);
    int hintW = canvas.textWidth(line3);

    int titleX = (Hal::DISPLAY_W - titleW) / 2;
    int bodyX = (Hal::DISPLAY_W - bodyW) / 2;
    int hintX = (Hal::DISPLAY_W - hintW) / 2;

    PixelRenderer::fillRect(42, 45, 156, 72, PixelRenderer::BLACK);
    PixelRenderer::drawPixelText(titleX + 1, titleY + 1, UiStrings::VISIT_AWAY_TITLE, PixelRenderer::BLACK, titleFs);
    PixelRenderer::drawPixelText(titleX, titleY, UiStrings::VISIT_AWAY_TITLE, PixelRenderer::WHITE, titleFs);
    PixelRenderer::drawPixelText(bodyX + 1, bodyY + 1, line2, PixelRenderer::BLACK, bodyFs);
    PixelRenderer::drawPixelText(bodyX, bodyY, line2, PixelRenderer::YELLOW, bodyFs);
    PixelRenderer::drawPixelText(hintX, hintY, line3, PixelRenderer::GRAY, 1.0f);
}

void TerrariumScene::drawVisitRecallConfirm() {
    LGFX_Sprite& canvas = Hal::ins().canvas();
    int x = 38;
    int y = 48;
    int w = 164;
    int h = 54;
    PixelRenderer::fillRect(x, y, w, h, PixelRenderer::BLACK);
    PixelRenderer::fillRect(x + 2, y + 2, w - 4, h - 4, 0x2104);

    float titleFs = PixelRenderer::getContentFontScale();
    PixelRenderer::applyTextStyle(titleFs);
    int titleW = canvas.textWidth(UiStrings::VISIT_RECALL_CONFIRM);
    int titleX = x + (w - titleW) / 2;
    PixelRenderer::drawPixelText(titleX, y + 12, UiStrings::VISIT_RECALL_CONFIRM, PixelRenderer::WHITE, titleFs);

    PixelRenderer::applyTextStyle(1.0f);
    int navW = canvas.textWidth(UiStrings::VISIT_RECALL_NAV);
    int navX = x + (w - navW) / 2;
    PixelRenderer::drawPixelText(navX, y + 34, UiStrings::VISIT_RECALL_NAV, PixelRenderer::YELLOW, 1.0f);
}

void TerrariumScene::drawDeathScreen() {
    PixelRenderer::fillRect(40, 40, 160, 55, PixelRenderer::BLACK);
    PixelRenderer::fillRect(45, 45, 150, 45, PixelRenderer::RED);
    PixelRenderer::drawPixelText(55, 55, UiStrings::DIED, PixelRenderer::WHITE, 3);
    PixelRenderer::drawPixelText(55, 80, UiStrings::HOLD_AB_RESET, PixelRenderer::WHITE, 1);
}

// 倾斜检测：每帧读取 IMU，防抖 200ms 后触发
void TerrariumScene::updateTilt() {
    float ax, ay, az;
    Hal::ins().getAccel(ax, ay, az);

    // 垂直方向过滤：竖屏/举起时忽略，避免甲虫乱跑
    if (fabsf(ay) > 1.5f) {
        static uint32_t lastLog = 0;
        if (Hal::ins().millis() - lastLog >= 1000) {
            Serial.printf("[Tilt] ignore, ay=%.2f (vertical)\n", ay);
            lastLog = Hal::ins().millis();
        }
        return;
    }

    // 摇晃优先：剧烈摇晃时跳过倾斜，避免冲突
    float mag = Hal::ins().getAccelMagnitude();
    if (mag > 2.0f) {
        static uint32_t lastShakeLog = 0;
        if (Hal::ins().millis() - lastShakeLog >= 500) {
            Serial.printf("[Tilt] ignore, mag=%.2f (shake)\n", mag);
            lastShakeLog = Hal::ins().millis();
        }
        return;
    }

    TiltDir newDir = TiltDir::NONE;
    if (ax > TILT_THRESHOLD_G) newDir = TiltDir::LEFT;
    else if (ax < -TILT_THRESHOLD_G) newDir = TiltDir::RIGHT;

    const char* highSideStr = (newDir == TiltDir::LEFT) ? "RIGHT" :
                              (newDir == TiltDir::RIGHT) ? "LEFT" : "NONE";
    const char* lowSideStr  = (newDir == TiltDir::LEFT) ? "LEFT" :
                              (newDir == TiltDir::RIGHT) ? "RIGHT" : "NONE";

    // 方向变化时打印：明确显示高低方向
    if (newDir != pendingTiltDir) {
        Serial.printf("[Tilt] ax=%.2f pending low=%s/%d -> low=%s/%d  (high=%s)\n",
                      ax,
                      (pendingTiltDir == TiltDir::LEFT) ? "LEFT" :
                      (pendingTiltDir == TiltDir::RIGHT) ? "RIGHT" : "NONE",
                      (int)pendingTiltDir,
                      lowSideStr, (int)newDir, highSideStr);
        pendingTiltDir = newDir;
        tiltStableMs = Hal::ins().millis();
    }

    if (newDir != activeTiltDir &&
        Hal::ins().millis() - tiltStableMs >= TILT_DEBOUNCE_MS) {
        activeTiltDir = newDir;
        Serial.printf("[Tilt] STABLE low=%s/%d high=%s (stable %dms)\n",
                      lowSideStr, (int)activeTiltDir, highSideStr, TILT_DEBOUNCE_MS);
        if (activeTiltDir != TiltDir::NONE) {
            onTilt(activeTiltDir, mag);
        } else {
            // 恢复水平：如果正在爬坡，停下来休息
            if (adultState == AdultState::CLIMB) {
                Serial.println("[Tilt] level, stop climb -> IDLE");
                adultState = AdultState::IDLE;
                stateTimer = 0;
                setIdleDuration();
            }
        }
    }
}

// 倾斜触发：
// - 小角度：转身朝高处，然后缓慢爬行或原地不动
// - 大角度：先向低处滑落一小段，再转身朝高处爬行
// dir 表示低处方向（设备哪一侧向下）
void TerrariumScene::onTilt(TiltDir dir, float magnitude) {
    mind.onTilted(Hal::ins().millis());
    const char* lowSideStr  = (dir == TiltDir::LEFT) ? "LEFT" : "RIGHT";
    const char* highSideStr = (dir == TiltDir::LEFT) ? "RIGHT" : "LEFT";
    Serial.printf("[Tilt] onTilt low=%s high=%s mag=%.2f adultState=%d bugX=%d\n",
                  lowSideStr, highSideStr, magnitude, (int)adultState, bugX);

    if (GameEngine::ins().isVisitHost()) {
        applyTiltToVisitor(dir, magnitude);
    }

    if (adultState == AdultState::EAT) {
        Serial.println("[Tilt] ignored: eating");
        return;
    }

    if (adultState == AdultState::REST && magnitude <= TILT_SLIDE_THRESHOLD_G) {
        Serial.println("[Tilt] ignored: resting");
        return;
    }

    // threaten（威吓）和 REST（夜间休息）期间只响应大角度滑落，
    // 小角度爬行被忽略；大角度滑落会唤醒休息中的甲虫。
    if (pokeThreatenEndMs != 0 && Hal::ins().millis() < pokeThreatenEndMs) {
        if (magnitude <= TILT_SLIDE_THRESHOLD_G) {
            Serial.println("[Tilt] ignored: threatening");
            return;
        }
    }

    // 低处在左 => 高处在右；低处在右 => 高处在左
    bool lowIsLeft = (dir == TiltDir::LEFT);
    tiltHighSideIsRight = !lowIsLeft;

    // 高处目标位置
    int highTarget = bugX + (tiltHighSideIsRight ? TILT_CLIMB_DISTANCE : -TILT_CLIMB_DISTANCE);
    if (highTarget < MIN_X) highTarget = MIN_X;
    if (highTarget > MAX_X) highTarget = MAX_X;
    climbTargetX = highTarget;

    if (magnitude > TILT_SLIDE_THRESHOLD_G) {
        // 大角度：先向低处滑落；若从夜间休息中滑落，设置随机清醒冷却
        if (adultState == AdultState::REST) {
            restResumeAllowedMs = Hal::ins().millis() + random(REST_WAKEUP_COOLDOWN_MIN_MS,
                                                               REST_WAKEUP_COOLDOWN_MAX_MS + 1);
        }
        int lowTarget = bugX + (lowIsLeft ? -TILT_SLIDE_DISTANCE : +TILT_SLIDE_DISTANCE);
        if (lowTarget < MIN_X) lowTarget = MIN_X;
        if (lowTarget > MAX_X) lowTarget = MAX_X;
        slideTargetX = lowTarget;

        Serial.printf("[Tilt] slide first -> lowTarget=%d then climb=%d\n",
                      slideTargetX, climbTargetX);

        bool needFaceRight = !tiltHighSideIsRight;  // 面向低处
        if (needFaceRight != faceRight) {
            startTurn(needFaceRight, true);
            slideAfterTurn = true;
        } else {
            adultState = AdultState::SLIDE;
            stateTimer = 0;
            stateDuration = 0;
        }
    } else {
        // 小角度：转身朝高处，然后爬行或不动
        Serial.printf("[Tilt] climb/highSide -> target=%d\n", climbTargetX);

        bool needFaceRight = tiltHighSideIsRight;
        if (needFaceRight != faceRight) {
            startTurn(needFaceRight, true);
            climbAfterTurn = true;
        } else {
            startClimbOrIdle();
        }
    }
}

// 小角度倾斜：优先向高处缓慢爬行，偶尔原地休息
void TerrariumScene::startClimbOrIdle() {
    if (random(100) < 45) {
        Serial.printf("[Tilt] start CLIMB -> target=%d\n", climbTargetX);
        adultState = AdultState::CLIMB;
        stateTimer = 0;
        stateDuration = 0;
    } else {
        Serial.println("[Tilt] stay IDLE at high side");
        adultState = AdultState::IDLE;
        stateTimer = 0;
        setIdleDuration();
    }
}

// 幼虫被戳：三段身体收缩 2 像素
void TerrariumScene::drawLarvaPoked(int x, int y, uint8_t palette) {
    int shake = ((Hal::ins().millis() / 70) % 2) ? 2 : -2;
    drawLarva(x + shake, y, palette);
}

int TerrariumScene::getPokeTargetX() const {
    return pokeFingerTargetVisitor && visitor.active ? visitor.x : bugX;
}

int TerrariumScene::getPokeTargetY() const {
    if (pokeFingerTargetVisitor && visitor.active) {
        return GROUND_Y - 20;
    }

    Bug& bug = GameEngine::ins().getBug();
    switch (bug.getStage()) {
        case Stage::EGG:
            return GROUND_Y - 7;
        case Stage::PUPA:
            return 95;
        case Stage::LARVA:
            return GROUND_Y - 5;
        case Stage::JUVENILE:
        case Stage::ADULT:
        default:
            return GROUND_Y - 20;
    }
}

void TerrariumScene::startPokeFeedback(uint32_t now, uint32_t durationMs, bool wasPoked, bool targetVisitor) {
    pokeFingerStartMs = now;
    pokeReactionEndMs = now + durationMs;
    pokeReactionWasPoked = wasPoked;
    pokeFingerTargetVisitor = targetVisitor;
    if (wasPoked) {
        pokeFingerFromRight = random(2) == 0;
        pokeFingerFrameIndex = random(ActionAssets::FINGER_FRAME_COUNT);
        pokeFingerYOffset = (int8_t)random(-3, 4);
    }
}

void TerrariumScene::drawPokeAction() {
    if (!pokeReactionWasPoked || pokeReactionEndMs == 0) return;

    uint32_t now = Hal::ins().millis();
    if (now >= pokeReactionEndMs) return;

    uint32_t duration = pokeReactionEndMs - pokeFingerStartMs;
    if (duration == 0) return;

    uint32_t elapsed = now - pokeFingerStartMs;
    if (elapsed > duration) elapsed = duration;

    // 先伸入，再回收，形成一次明确的“戳”位移动画。
    const uint32_t pushDuration = (duration * 45) / 100;
    const int travel = 14;
    int advance;
    if (elapsed <= pushDuration || pushDuration == 0) {
        advance = (pushDuration == 0) ? travel : (int)((elapsed * travel) / pushDuration);
    } else {
        uint32_t retractElapsed = elapsed - pushDuration;
        uint32_t retractDuration = duration - pushDuration;
        advance = travel - (int)((retractElapsed * travel) / retractDuration);
    }
    if (advance < 0) advance = 0;
    if (advance > travel) advance = travel;

    uint8_t frameIndex = pokeFingerFrameIndex;
    if (frameIndex >= ActionAssets::FINGER_FRAME_COUNT) frameIndex = 0;
    uint16_t offset = pgm_read_word(&ActionAssets::FINGER_FRAMES[frameIndex].offset);
    uint16_t length = pgm_read_word(&ActionAssets::FINGER_FRAMES[frameIndex].length);

    static constexpr int CONTACT_OFFSET_X = 18;

    const int targetX = getPokeTargetX() + (pokeFingerFromRight ? CONTACT_OFFSET_X : -CONTACT_OFFSET_X);
    const int targetY = getPokeTargetY() + pokeFingerYOffset;
    uint8_t frameW = pgm_read_byte(&ActionAssets::FINGER_FRAMES[frameIndex].width);
    uint8_t frameH = pgm_read_byte(&ActionAssets::FINGER_FRAMES[frameIndex].height);
    uint8_t tipX = pgm_read_byte(&ActionAssets::FINGER_FRAMES[frameIndex].tipX);
    uint8_t tipY = pgm_read_byte(&ActionAssets::FINGER_FRAMES[frameIndex].tipY);
    if (pokeFingerFromRight) {
        tipX = frameW - 1 - tipX;
    }

    int x;
    if (pokeFingerFromRight) {
        x = targetX - tipX + travel - advance;
    } else {
        x = targetX - tipX - travel + advance;
    }
    int y = targetY - tipY;

    PixelRenderer::drawRgb565Rle(x, y,
                                 frameW,
                                 frameH,
                                 ActionAssets::FINGER_RLE,
                                 offset, length,
                                 pokeFingerFromRight);
}

// 戳冷却提示：甲虫上方闪烁小点
void TerrariumScene::drawPokeCooldownHint(int x, int y) {
    uint16_t c = ((Hal::ins().millis() / 100) % 2) ? PixelRenderer::GRAY : PixelRenderer::BLACK;
    int hintY = y - 16;
    if (hintY < 0) hintY = 0;
    PixelRenderer::fillRect(x - 2, hintY, 4, 4, c);
}
