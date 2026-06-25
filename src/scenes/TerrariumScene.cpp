#include "TerrariumScene.h"
#include "../core/GameEngine.h"
#include "../core/UiStrings.h"
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

bool isMobileBeetleStage(Stage stage) {
    return stage == Stage::ADULT || stage == Stage::JUVENILE;
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
        adultState = saved.adultState <= (uint8_t)AdultState::REST ?
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
        return;
    }

    resetLocalViewState();
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
    toyChargeStartMs = 0;
    toyChargeDurationMs = 0;
    toyChargeDir = 1;
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

void TerrariumScene::updateToyPhysics(uint32_t nowMs) {
    if (!isToyEnabled()) {
        toyVisible = false;
        toyEntryActive = false;
        toyCharging = false;
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

    Bug& bug = GameEngine::ins().getBug();
    if (adultState == AdultState::REST || adultState == AdultState::EAT) return;

    float adultScale = bug.getAdultScale();
    float beetleHalfW = 34.0f * adultScale;
    float beetleTop = GROUND_Y - 30.0f * adultScale;
    float beetleBottom = GROUND_Y - 2.0f;
    float beetleLeft = bugX - beetleHalfW;
    float beetleRight = bugX + beetleHalfW;

    float closestX = toyX;
    if (closestX < beetleLeft) closestX = beetleLeft;
    if (closestX > beetleRight) closestX = beetleRight;
    float closestY = toyY;
    if (closestY < beetleTop) closestY = beetleTop;
    if (closestY > beetleBottom) closestY = beetleBottom;

    float dx = toyX - closestX;
    float dy = toyY - closestY;
    if (dx * dx + dy * dy <= (float)(spec.radius * spec.radius)) {
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

bool TerrariumScene::startToyButtonInteraction(uint32_t nowMs) {
    if (!isToyEnabled()) return false;
    if (toyEntryActive) return true;
    toyType = toyTypeForStyle(GameEngine::ins().getToyStyle());
    ToyButtonInteraction interaction = toyButtonInteractionForStyle(GameEngine::ins().getToyStyle());
    switch (interaction) {
        case ToyButtonInteraction::THROW_ARC:
            startToyArcThrow(nowMs);
            return true;
        case ToyButtonInteraction::DROP_DOWN:
            startToyDrop(nowMs);
            return true;
        case ToyButtonInteraction::POKE:
        default:
            return false;
    }
}

void TerrariumScene::startToyArcThrow(uint32_t nowMs) {
    if (!isToyEnabled()) return;
    const ToySpec& spec = currentToySpec();
    toyVisible = false;
    toyCharging = false;
    toyEntryActive = true;
    toyEntryInteraction = ToyButtonInteraction::THROW_ARC;
    toyEntryStartMs = nowMs;
    toyThrowFromX = 120 + random(-42, 43);
    toyThrowFromY = Hal::DISPLAY_H + 18;
    toyThrowTargetX = bugX + random(-24, 25);
    toyThrowTargetY = GROUND_Y - (int)(42.0f * GameEngine::ins().getBug().getAdultScale()) +
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

void TerrariumScene::startToyDrop(uint32_t nowMs) {
    if (!isToyEnabled()) return;
    const ToySpec& spec = currentToySpec();
    toyVisible = false;
    toyCharging = false;
    toyEntryActive = true;
    toyEntryInteraction = ToyButtonInteraction::DROP_DOWN;
    toyEntryStartMs = nowMs;
    toyThrowTargetX = bugX + random(-18, 19);
    toyThrowTargetY = GROUND_Y - (int)(40.0f * GameEngine::ins().getBug().getAdultScale()) +
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

    int pushDir = toyX >= bugX ? 1 : -1;
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

    pokeReactionStartMs = nowMs;
    pokeReactionWasPoked = true;
    pokeThreatenEndMs = nowMs + POKE_REACTION_MS;
    alertUntilMs = nowMs + random(ALERT_MIN_MS, ALERT_MAX_MS + 1);
    adultState = AdultState::IDLE;
    stateTimer = 0;
    setIdleDuration();
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

    Bug& bug = GameEngine::ins().getBug();
    uint32_t nowMs = Hal::ins().millis();
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
    if (isMobileBeetleStage(bug.getStage()) && !bug.isDead()) {
        drawToy();
    }
    drawBug();
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
        if (isMobileBeetleStage(bug.getStage()) &&
            startToyButtonInteraction(Hal::ins().millis())) {
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
        bool ok = bug.poke(gameNow);
        if (ok) {
            uint32_t now = Hal::ins().millis();
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
    adultState = AdultState::TURN;
    turnTargetFaceRight = targetFaceRight;
    walkAfterTurn = continueWalking;
    if (!continueWalking) walkTargetIsRest = false;
    turnFrameIndex = random(HerculesAdultSprites::TURN_FRAME_COUNT);
    stateTimer = 0;
    stateDuration = TURN_DURATION_FRAMES;
}

// 开始向目标位置行走
void TerrariumScene::startWalkTo(int x, bool restTarget) {
    targetX = x;
    if (targetX < MIN_X) targetX = MIN_X;
    if (targetX > MAX_X) targetX = MAX_X;
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

    uint16_t hColor = bug.getHunger() > 50 ? PixelRenderer::GREEN :
                      (bug.getHunger() > 20 ? PixelRenderer::YELLOW : PixelRenderer::RED);
    int barX = heartX;
    int barW = (weatherX + ICON_SIZE) - heartX;
    PixelRenderer::drawProgressBar(barX, barY, barW, barH, bug.getHunger() / 100.0f, hColor, PixelRenderer::GRAY);
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

int TerrariumScene::getPokeTargetY() const {
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

void TerrariumScene::startPokeFeedback(uint32_t now, uint32_t durationMs, bool wasPoked) {
    pokeFingerStartMs = now;
    pokeReactionEndMs = now + durationMs;
    pokeReactionWasPoked = wasPoked;
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

    const int targetX = bugX + (pokeFingerFromRight ? CONTACT_OFFSET_X : -CONTACT_OFFSET_X);
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
