#include "BugMind.h"

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
    uint8_t fatigue = 0;
    if (idleSec > 60) fatigue = (uint8_t)(idleSec / 8); // 每8秒+1
    if (mot < 30) fatigue += 55;
    else if (mot < 60) fatigue += 25;

    energyNeed = fatigue;
    if (isNight) energyNeed += 50;
    if (woodPlaced) energyNeed += 18;
    energyNeed = clamp(energyNeed);

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
    curiosityNeed = (uint8_t)(boredSec * 3);
    if (curiosityNeed > 140) curiosityNeed = 140;
    if (hunger > 70 && mot > 50) curiosityNeed += 35; // 饱+有精力更想探索
    if (isNight) curiosityNeed = curiosityNeed / 2;   // 夜间好奇心降低
    curiosityNeed = clamp(curiosityNeed);

    // ---- 基础欲望分数 ----
    desireScores[(uint8_t)Desire::EAT]    = foodAvailable ? hungerNeed : 0;
    desireScores[(uint8_t)Desire::REST]   = energyNeed;
    desireScores[(uint8_t)Desire::WANDER] = curiosityNeed;
    desireScores[(uint8_t)Desire::HIDE]   = safetyNeed;
    desireScores[(uint8_t)Desire::STARE]  = 35; // 基础发呆倾向

    // ---- 当前欲望惯性（避免频繁切换） ----
    desireScores[(uint8_t)topDesire_] += INERTIA_BONUS;

    // ---- 随机噪声（让每只甲虫"个性"不同） ----
    for (int i = 0; i < (int)Desire::COUNT; i++) {
        desireScores[i] += (uint8_t)(random(RANDOM_NOISE_MAX));
    }

    // ---- 选择最高分 ----
    uint8_t max = 0;
    Desire winner = Desire::STARE;
    for (int i = 0; i < (int)Desire::COUNT; i++) {
        if (desireScores[i] > max) {
            max = desireScores[i];
            winner = (Desire)i;
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
            if (topDesire_ == Desire::EAT) return "肚子咕咕叫...去找点吃的";
            return "好饿...有什么能吃的吗";
        case Mood::SLEEPY:
            if (topDesire_ == Desire::REST) return "眼皮好重...该趴会儿了";
            return "困...但还是想再走走";
        case Mood::ALERT:
            if (topDesire_ == Desire::HIDE) return "刚才那是什么！？小心为上";
            return "保持警觉...别放松";
        case Mood::ANGRY:
            return "哼！别碰我！";
        case Mood::BORED:
            if (topDesire_ == Desire::WANDER) return "到处走走吧，这里太无聊了";
            return "...发呆也挺好的";
        case Mood::CURIOUS:
            if (topDesire_ == Desire::WANDER) return "那边好像有什么？去看看";
            return "嗯？有点意思";
        case Mood::EXCITED:
            if (topDesire_ == Desire::WANDER) return "精力满满！出发！";
            return "今天状态真不错";
        case Mood::CALM:
        default:
            if (topDesire_ == Desire::STARE) return "...";
            if (topDesire_ == Desire::EAT) return "该补充点能量了";
            if (topDesire_ == Desire::REST) return "休息一下吧";
            return "这里很安静";
    }
}

const char* BugMind::moodName() const {
    switch (mood_) {
        case Mood::CALM:     return "CALM";
        case Mood::HUNGRY:   return "HUNGRY";
        case Mood::SLEEPY:   return "SLEEPY";
        case Mood::ALERT:    return "ALERT";
        case Mood::BORED:    return "BORED";
        case Mood::CURIOUS:  return "CURIOUS";
        case Mood::ANGRY:    return "ANGRY";
        case Mood::EXCITED:  return "EXCITED";
        default:             return "?";
    }
}

const char* BugMind::desireName() const {
    switch (topDesire_) {
        case Desire::EAT:    return "EAT";
        case Desire::REST:   return "REST";
        case Desire::WANDER: return "WANDER";
        case Desire::STARE:  return "STARE";
        case Desire::HIDE:   return "HIDE";
        default:             return "?";
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
