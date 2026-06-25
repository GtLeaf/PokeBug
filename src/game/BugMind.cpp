#include "BugMind.h"
#include "../core/UiStrings.h"

BugMind::BugMind() {}

void BugMind::update(uint8_t hunger, uint8_t mot, bool isNight, bool woodPlaced,
                     bool foodAvailable, uint32_t realNow) {
    computeDesires(hunger, mot, isNight, woodPlaced, foodAvailable, realNow);
    computeMood(hunger, mot, isNight);
    lastUpdateMs = realNow;
}

void BugMind::computeDesires(uint8_t hunger, uint8_t mot, bool isNight,
                             bool woodPlaced, bool foodAvailable, uint32_t realNow) {
    // ---- 饥饿需求 ----
    if (hunger < 30) hungerNeed = 200;
    else if (hunger < 60) hungerNeed = 160 - (hunger - 30) * 2;
    else hungerNeed = (uint8_t)(100 - (hunger - 60));

    // ---- 精力/休息需求 ----
    uint32_t idleSec = (realNow - lastActivityMs) / 1000;
    uint16_t fatigue = 0;
    if (idleSec > 60) fatigue = idleSec / 8; // 每8秒+1
    if (mot < 30) fatigue += 55;
    else if (mot < 60) fatigue += 25;

    if (isNight) fatigue += 50;
    if (woodPlaced) fatigue += 18;
    energyNeed = clamp(fatigue);

    // ---- 安全需求 ----
    safetyNeed = 0;
    if (realNow < alertUntilMs) {
        uint32_t remain = alertUntilMs - realNow;
        safetyNeed = (uint8_t)(clamp((remain * 220) / 16000));
    } else if (pokedAtMs != 0 && realNow - pokedAtMs < 3000) {
        safetyNeed = 60; // 被戳后短时间内保持低警觉
    }

    // ---- 好奇需求 ----
    uint32_t boredSec = (realNow - lastActivityMs) / 1000;
    uint16_t curiosity = boredSec * 3;
    if (curiosity > 140) curiosity = 140;
    if (hunger > 70 && mot > 50) curiosity += 35; // 饱+有精力更想探索
    if (isNight) curiosity = curiosity / 2;       // 夜间好奇心降低
    curiosityNeed = clamp(curiosity);

    // ---- 基础欲望分数 ----
    uint16_t baseScores[(uint8_t)Desire::COUNT] = {0};
    bool sated = hunger >= 80;
    baseScores[(uint8_t)Desire::EAT]    = foodAvailable ? (sated ? 5 : hungerNeed) : 0;
    baseScores[(uint8_t)Desire::REST]   = energyNeed;
    baseScores[(uint8_t)Desire::WANDER] = curiosityNeed;
    baseScores[(uint8_t)Desire::HIDE]   = safetyNeed;
    baseScores[(uint8_t)Desire::STARE]  = 35; // 基础发呆倾向

    // ---- 当前欲望惯性与随机噪声（饱和计算，避免 uint8_t 回绕） ----
    for (int i = 0; i < (int)Desire::COUNT; i++) {
        uint16_t score = baseScores[i];
        if (i == (int)topDesire_ && !(sated && i == (int)Desire::EAT)) score += INERTIA_BONUS;
        score += (uint16_t)random(RANDOM_NOISE_MAX);
        desireScores[i] = clamp(score);
    }

    // ---- 选择最高分 ----
    uint8_t maxScore = 0;
    Desire winner = Desire::STARE;
    for (int i = 0; i < (int)Desire::COUNT; i++) {
        if (desireScores[i] > maxScore) {
            maxScore = desireScores[i];
            winner = (Desire)i;
        }
    }

    // ---- 切换阈值（让心智有一点"坚持己见"） ----
    if (winner != topDesire_) {
        uint8_t currentScore = desireScores[(uint8_t)topDesire_];
        if (maxScore < clamp((uint16_t)currentScore + SWITCH_MARGIN)) {
            winner = topDesire_;
        }
    }
    topDesire_ = winner;
}

void BugMind::computeMood(uint8_t hunger, uint8_t mot, bool isNight) {
    if (safetyNeed > 100) {
        mood_ = Mood::ALERT;
    } else if (safetyNeed > 50) {
        mood_ = Mood::ANGRY;
    } else if (energyNeed > 100 && hungerNeed > 100) {
        mood_ = Mood::SLEEPY; // 又饿又困 -> 优先困倦
    } else if (energyNeed > 90) {
        mood_ = Mood::SLEEPY;
    } else if (hungerNeed > 90) {
        mood_ = Mood::HUNGRY;
    } else if (curiosityNeed > 90) {
        mood_ = Mood::CURIOUS;
    } else if (curiosityNeed > 50) {
        mood_ = Mood::BORED;
    } else if (mot > 80 && hunger > 75) {
        mood_ = Mood::EXCITED;
    } else {
        mood_ = Mood::CALM;
    }
}

const char* BugMind::innerVoice() const {
    switch (mood_) {
        case Mood::HUNGRY:
            if (topDesire_ == Desire::EAT) return UiStrings::MIND_HUNGRY_EAT;
            return UiStrings::MIND_HUNGRY_IDLE;
        case Mood::SLEEPY:
            if (topDesire_ == Desire::REST) return UiStrings::MIND_SLEEPY_REST;
            return UiStrings::MIND_SLEEPY_IDLE;
        case Mood::ALERT:
            if (topDesire_ == Desire::HIDE) return UiStrings::MIND_ALERT_HIDE;
            return UiStrings::MIND_ALERT_IDLE;
        case Mood::ANGRY:
            return UiStrings::MIND_ANGRY;
        case Mood::BORED:
            if (topDesire_ == Desire::WANDER) return UiStrings::MIND_BORED_WANDER;
            return UiStrings::MIND_BORED_IDLE;
        case Mood::CURIOUS:
            if (topDesire_ == Desire::WANDER) return UiStrings::MIND_CURIOUS_WANDER;
            return UiStrings::MIND_CURIOUS_IDLE;
        case Mood::EXCITED:
            if (topDesire_ == Desire::WANDER) return UiStrings::MIND_EXCITED_WANDER;
            return UiStrings::MIND_EXCITED_IDLE;
        case Mood::CALM:
        default:
            if (topDesire_ == Desire::STARE) return UiStrings::MIND_CALM_STARE;
            if (topDesire_ == Desire::EAT) return UiStrings::MIND_CALM_EAT;
            if (topDesire_ == Desire::REST) return UiStrings::MIND_CALM_REST;
            return UiStrings::MIND_CALM_IDLE;
    }
}

const char* BugMind::moodName() const {
    switch (mood_) {
        case Mood::CALM:     return UiStrings::MOOD_CALM;
        case Mood::HUNGRY:   return UiStrings::MOOD_HUNGRY;
        case Mood::SLEEPY:   return UiStrings::MOOD_SLEEPY;
        case Mood::ALERT:    return UiStrings::MOOD_ALERT;
        case Mood::BORED:    return UiStrings::MOOD_BORED;
        case Mood::CURIOUS:  return UiStrings::MOOD_CURIOUS;
        case Mood::ANGRY:    return UiStrings::MOOD_ANGRY;
        case Mood::EXCITED:  return UiStrings::MOOD_EXCITED;
        default:             return UiStrings::UNKNOWN_SHORT;
    }
}

const char* BugMind::desireName() const {
    switch (topDesire_) {
        case Desire::EAT:    return UiStrings::DESIRE_EAT;
        case Desire::REST:   return UiStrings::DESIRE_REST;
        case Desire::WANDER: return UiStrings::DESIRE_WANDER;
        case Desire::STARE:  return UiStrings::DESIRE_STARE;
        case Desire::HIDE:   return UiStrings::DESIRE_HIDE;
        default:             return UiStrings::UNKNOWN_SHORT;
    }
}

// ---- 外部事件 ----

void BugMind::onPoked(uint32_t realNow) {
    pokedAtMs = realNow;
    alertUntilMs = realNow + 12000; // 12秒警戒期
    lastActivityMs = realNow;
}

void BugMind::onShaken(uint32_t realNow) {
    shakeAtMs = realNow;
    alertUntilMs = realNow + 6000;  // 摇晃也触发6秒警戒
    lastActivityMs = realNow;
}

void BugMind::onTilted(uint32_t realNow) {
    tiltAtMs = realNow;
    lastActivityMs = realNow;
}

void BugMind::onAte(uint32_t realNow) {
    ateAtMs = realNow;
    lastActivityMs = realNow;
    alertUntilMs = 0; // 进食后放松警戒
}

void BugMind::onRested(uint32_t realNow) {
    lastActivityMs = realNow;
}

void BugMind::resetActivityTimer(uint32_t realNow) {
    lastActivityMs = realNow;
}
