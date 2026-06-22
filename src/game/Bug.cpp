#include "Bug.h"
#include "../core/UiStrings.h"
#include <Arduino.h>
#include <cmath>

Bug::Bug() {}

void Bug::initNew(uint64_t now) {
    randomizeGenes();

    siz = 1.0f; str = 1.0f; end = 1.0f; spd = 1.0f; spi = 1.0f;
    mot = 50;
    hunger = 100;
    stage = Stage::EGG;
    alive = true;

    for (int i = 0; i < (int)FoodType::COUNT; i++) foodCounts[i] = 0;
    foodCounts[(uint8_t)FoodType::DROP] = 6; // 初始 6 份 Drop
    rottenWood = 0;
    woodPlaced = false;
    foodInTray = false;
    trayFoodType = FoodType::DROP;
    foodAmount = 0;
    lastEatTime = 0;

    stageStartTime = now;
    eggStartTime = now;
    lastFeedTime = 0;
    lastSapProduceTime = now;
    lastShakeTrainTime = 0;
    eggShakeDelayAcc = 0;
    restStartTime = 0;
    woodRestAcc = 0;
    lastPokeTime = 0;

    pokeAnger = 0;
    motBuffAmount = 0;
    motBuffEndTime = 0;

    temperament = Temperament::SPIRIT;
    eggShakeCount = 0;
    eggViolentShakeCount = 0;
    eggPokeCount = 0;
    eggWaterCount = 0;
    eggLeftTiltMs = 0;
    eggRightTiltMs = 0;
    eggLastAction = 0;

    foodTrayLevel = 1;
    foodTrayType = FoodType::DROP;
    woodStyle = 0;

    larvaCitrusCount = 0;
    larvaBerryFed = false;

    larvaFeeds = 0;
    pupaShakes = 0;
    wins = 0;
    losses = 0;
    generation = 0;

    lastUpdateTime = now;
    hungerDropAcc = 0;
    deathTimerStart = 0;
}

void Bug::randomizeGenes() {
    auto rollGene = []() -> uint8_t {
        uint8_t dom = random(16);
        uint8_t rec = random(16);
        return (dom << 4) | rec;
    };
    geneVIG = rollGene();
    geneATK = rollGene();
    geneMNT = rollGene();
    geneEND = rollGene();
    geneAPP = rollGene();
}

void Bug::modHunger(int8_t delta) {
    int h = (int)hunger + delta;
    if (h < 0) h = 0;
    if (h > 100) h = 100;
    hunger = (uint8_t)h;
}

void Bug::addTrainingBonus(float sizDelta, float strDelta, float endDelta,
                           float spdDelta, float spiDelta) {
    siz += sizDelta;
    str += strDelta;
    end += endDelta;
    spd += spdDelta;
    spi += spiDelta;
    clampAttributes();
}

void Bug::clampAttributes() {
    if (siz < 1.0f) siz = 1.0f;
    if (siz > sizCap()) siz = sizCap();
    if (str < 1.0f) str = 1.0f;
    if (str > strCap()) str = strCap();
    if (end < 1.0f) end = 1.0f;
    if (end > endCap()) end = endCap();
    if (spd < 1.0f) spd = 1.0f;
    if (spd > spdCap()) spd = spdCap();
    if (spi < 1.0f) spi = 1.0f;
    if (spi > spiCap()) spi = spiCap();
    if (mot > 100) mot = 100;
    if (hunger > 100) hunger = 100;
}

void Bug::update(uint64_t now) {
    if (!alive || now <= lastUpdateTime) return;

    uint32_t delta = (uint32_t)(now - lastUpdateTime);
    lastUpdateTime = now;

    updateHunger(now, delta);
    if (!alive) return;

    // MOT buff 过期恢复
    if (motBuffAmount > 0 && now >= motBuffEndTime) {
        if (mot > motBuffAmount) mot -= motBuffAmount;
        else mot = 0;
        Serial.printf("[Bug] MOT buff expired, -%d, mot=%d\n", motBuffAmount, mot);
        motBuffAmount = 0;
        motBuffEndTime = 0;
    }

    // 蛹期安静成长 SPI
    if (stage == Stage::PUPA) {
        updatePupaSpi(now, delta);
    }

    // 成虫产树汁（保留原有产出机制）
    if (stage == Stage::ADULT) {
        if (now - lastSapProduceTime >= ADULT_SAP_PRODUCE_MS && foodCounts[(uint8_t)FoodType::DROP] < MAX_FOOD_COUNT) {
            foodCounts[(uint8_t)FoodType::DROP]++;
            lastSapProduceTime = now;
        }
    }

    // 幼虫/蛹期自主进食；成虫由 TerrariumScene 在走到食物盘并进入 EAT 状态时控制进食
    if (stage != Stage::ADULT) {
        eatFromTray(now);
    }

    // 阶段推进
    checkStageTransition(now);
    clampAttributes();
}

void Bug::updateHunger(uint64_t now, uint32_t deltaMs) {
    hungerDropAcc += deltaMs;
    while (hungerDropAcc >= HUNGER_DROP_MS) {
        hungerDropAcc -= HUNGER_DROP_MS;
        if (hunger > 0) {
            hunger--;
        }
    }

    if (hunger == 0) {
        if (deathTimerStart == 0) deathTimerStart = now;
        else if (now - deathTimerStart >= STARVE_DEATH_MS) {
            alive = false;
        }
    } else {
        deathTimerStart = 0;
    }

    if (hunger < 30 && mot > 50) mot = 50;
}

void Bug::updatePupaSpi(uint64_t now, uint32_t deltaMs) {
    static uint32_t pupaSpiAcc = 0;
    uint64_t elapsed = now > stageStartTime ? now - stageStartTime : 0;
    if (elapsed > PUPA_DURATION_MS) {
        uint64_t overrun = elapsed - PUPA_DURATION_MS;
        deltaMs = overrun >= deltaMs ? 0 : (uint32_t)(deltaMs - overrun);
    }

    uint32_t interval = PUPA_SPI_GROWTH_MS;
    if (larvaBerryFed) {
        interval = (interval * 9) / 10; // 提前 10%，但整个蛹期仍只会稳定触发一次
    }
    pupaSpiAcc += deltaMs;
    while (pupaSpiAcc >= interval) {
        pupaSpiAcc -= interval;
        spi += 1.0f;
    }
}

bool Bug::canAdvanceStage(uint64_t now) const {
    if (now <= stageStartTime) return false;
    uint64_t elapsed = now - stageStartTime;
    switch (stage) {
        case Stage::EGG:
            return elapsed >= EGG_DURATION_MS;
        case Stage::LARVA:
            return elapsed >= LARVA_DURATION_MS;
        case Stage::PUPA:
            return elapsed >= PUPA_DURATION_MS;
        case Stage::JUVENILE:
            return elapsed >= JUVENILE_DURATION_MS;
        default:
            return false;
    }
}

void Bug::checkStageTransition(uint64_t now) {
    uint8_t maxAdvances = 3;
    while (maxAdvances-- > 0 && canAdvanceStage(now)) {
        advanceStage(now);
    }
}

void Bug::advanceStage(uint64_t now) {
    (void)now;
    switch (stage) {
        case Stage::EGG:
            stageStartTime += EGG_DURATION_MS;
            stage = Stage::LARVA;
            temperament = determineTemperament(stageStartTime);
            // 孵化时给初始食物
            foodCounts[(uint8_t)FoodType::DROP] = 6;
            break;
        case Stage::LARVA:
            stageStartTime += LARVA_DURATION_MS;
            stage = Stage::PUPA;
            str += 1.0f;
            siz += 0.5f;
            rottenWood = 1;
            break;
        case Stage::PUPA:
            stageStartTime += PUPA_DURATION_MS;
            stage = Stage::JUVENILE;
            spi += 3.0f - (float)pupaShakes;
            break;
        case Stage::JUVENILE:
            stageStartTime += JUVENILE_DURATION_MS;
            stage = Stage::ADULT;
            break;
        default:
            return;
    }
    clampAttributes();
}

Temperament Bug::determineTemperament(uint64_t now) {
    uint64_t totalDuration = now - eggStartTime;
    if (totalDuration == 0) totalDuration = 1;
    uint32_t tiltPct = (uint32_t)((eggLeftTiltMs + eggRightTiltMs) * 100 / totalDuration);

    // L1: 迅捷
    if (tiltPct >= 30) return Temperament::SWIFT;

    // L2: 操作类候选
    struct Candidate { Temperament t; int count; };
    Candidate candidates[3];
    int n = 0;
    if (eggPokeCount >= 4)    candidates[n++] = {Temperament::RESILIENT, eggPokeCount};
    if (eggWaterCount >= 4)   candidates[n++] = {Temperament::GIANT, eggWaterCount};
    if (eggShakeCount >= 4 && eggViolentShakeCount >= 1)
        candidates[n++] = {Temperament::BRUTE, eggShakeCount};

    if (n > 0) {
        int maxCount = candidates[0].count;
        for (int i = 1; i < n; i++) {
            if (candidates[i].count > maxCount) maxCount = candidates[i].count;
        }
        // 平局：最近触发者胜出
        if (eggLastAction == (uint8_t)EggAction::POKE && eggPokeCount == maxCount)
            return Temperament::RESILIENT;
        if (eggLastAction == (uint8_t)EggAction::WATER && eggWaterCount == maxCount)
            return Temperament::GIANT;
        if (eggLastAction == (uint8_t)EggAction::SHAKE && eggShakeCount == maxCount && eggViolentShakeCount >= 1)
            return Temperament::BRUTE;
        // 回退固定优先级：戳 > 喷水 > 摇晃
        for (int i = 0; i < n; i++) {
            if (candidates[i].count == maxCount) return candidates[i].t;
        }
    }

    // L3: 均衡
    bool allInRange = (eggPokeCount >= 1 && eggPokeCount <= 3) &&
                      (eggWaterCount >= 1 && eggWaterCount <= 3) &&
                      (eggShakeCount >= 1 && eggShakeCount <= 3);
    if (allInRange) return Temperament::BALANCED;

    // L4: 灵心兜底
    return Temperament::SPIRIT;
}

const char* Bug::getTemperamentName() const {
    switch (temperament) {
        case Temperament::SWIFT:     return UiStrings::TEMP_SWIFT;
        case Temperament::RESILIENT: return UiStrings::TEMP_RESILIENT;
        case Temperament::GIANT:     return UiStrings::TEMP_GIANT;
        case Temperament::BRUTE:     return UiStrings::TEMP_BRUTE;
        case Temperament::BALANCED:  return UiStrings::TEMP_BALANCED;
        case Temperament::SPIRIT:    return UiStrings::TEMP_SPIRIT;
        default: return "?";
    }
}

float Bug::getAdultDepth() const {
    auto ratio = [](float value, float cap) -> float {
        if (cap <= 0.0f) return 0.0f;
        float r = value / cap;
        if (r < 0.0f) return 0.0f;
        if (r > 1.0f) return 1.0f;
        return r;
    };

    switch (temperament) {
        case Temperament::BRUTE:
            return ratio(str, strCap());
        case Temperament::SWIFT:
            return ratio(spd, spdCap());
        case Temperament::GIANT:
            return ratio(siz, sizCap());
        case Temperament::RESILIENT:
            return ratio(end, endCap());
        case Temperament::SPIRIT:
            return ratio(spi, spiCap());
        case Temperament::BALANCED:
            return (ratio(siz, sizCap()) +
                    ratio(str, strCap()) +
                    ratio(end, endCap()) +
                    ratio(spd, spdCap()) +
                    ratio(spi, spiCap())) / 5.0f;
    }
    return 0.0f;
}

float Bug::getAdultScale() const {
    if (temperament != Temperament::GIANT) return 1.0f;
    if (siz < 10.0f) return 0.9f;
    if (siz < 18.0f) return 1.1f;
    return 1.2f;
}

float Bug::getEnvMultiplier(int attrIndex) const {
    float mult = 1.0f;

    // 食物盘加成
    if (foodTrayLevel == 2) mult *= 1.03f;
    else if (foodTrayLevel == 3) mult *= 1.06f;

    // 食物倾向加成
    int foodAttr = FoodTypeInfo::envAttribute(foodTrayType);
    if (foodAttr == attrIndex) mult *= 1.03f;

    // 腐木加成（仅成虫）
    if (stage == Stage::ADULT && woodPlaced) {
        int woodAttr = -1;
        switch (woodStyle) {
            case 0: woodAttr = 1; break; // Twig -> STR
            case 1: woodAttr = 0; break; // Stack -> SIZ
            case 2: woodAttr = 4; break; // Mossy -> SPI
            case 3: woodAttr = 3; break; // Pale -> SPD
            case 4: woodAttr = 2; break; // Hollow -> END
            default: break;
        }
        if (woodAttr == attrIndex) mult *= 1.03f;
    }

    return mult;
}

void Bug::setFoodTray(uint8_t level, FoodType type) {
    foodTrayLevel = level;
    foodTrayType = type;
}

void Bug::setWood(uint8_t style) {
    woodStyle = style;
}

bool Bug::placeFoodInTray(FoodType type) {
    if (foodAmount > FOOD_MAX_AMOUNT / 2) return false;
    if (foodCounts[(uint8_t)type] == 0) return false;
    if (stage != Stage::LARVA && stage != Stage::JUVENILE && stage != Stage::ADULT) return false;
    if (FoodTypeInfo::level(type) > foodTrayLevel) return false;

    foodCounts[(uint8_t)type]--;
    trayFoodType = type;
    foodAmount = FOOD_MAX_AMOUNT;
    foodInTray = true;
    lastFeedTime = lastUpdateTime;
    return true;
}

void Bug::feed(FoodType type, uint64_t now) {
    const FoodTypeInfo::FoodStats& st = FoodTypeInfo::stats(type);

    // 阶段吸收系数
    float absSiz = 0.0f, absStr = 0.0f, absEnd = 0.0f, absSpd = 0.0f, absSpi = 0.0f;
    switch (stage) {
        case Stage::LARVA:
            absSiz = 1.0f; absStr = 1.0f; absSpd = 0.2f; absSpi = 0.1f;
            break;
        case Stage::JUVENILE:
            absSiz = 0.5f; absStr = 0.5f; absEnd = 0.5f; absSpd = 1.0f; absSpi = 0.5f;
            break;
        case Stage::ADULT:
            absSiz = 0.1f; absStr = 0.1f; absEnd = 0.1f; absSpd = 0.1f; absSpi = 0.1f;
            break;
        default:
            return;
    }

    // Jelly 间隔惩罚
    float penalty = 1.0f;
    if (type == FoodType::JELLY && (now - lastFeedTime) < FoodTypeInfo::JELLY_INTERVAL_MS) {
        penalty = 0.5f;
    }

    // 气质系数
    float tempSiz = 1.0f, tempStr = 1.0f, tempEnd = 1.0f, tempSpd = 1.0f, tempSpi = 1.0f;
    switch (temperament) {
        case Temperament::SWIFT:     tempSpd = 1.10f; tempSiz = 0.90f; break;
        case Temperament::RESILIENT: tempEnd = 1.10f; tempSpi = 0.90f; break;
        case Temperament::GIANT:     tempSiz = 1.10f; tempSpd = 0.90f; break;
        case Temperament::BRUTE:     tempStr = 1.10f; tempEnd = 0.90f; break;
        case Temperament::BALANCED:  break;
        case Temperament::SPIRIT:    tempSpi = 1.10f; tempStr = 0.90f; break;
    }

    // 环境系数
    float envSiz = getEnvMultiplier(0);
    float envStr = getEnvMultiplier(1);
    float envEnd = getEnvMultiplier(2);
    float envSpd = getEnvMultiplier(3);
    float envSpi = getEnvMultiplier(4);

    siz += st.siz * absSiz * tempSiz * envSiz * sizGrowthMult() * penalty;
    str += st.str * absStr * tempStr * envStr * strGrowthMult() * penalty;
    end += st.end * absEnd * tempEnd * envEnd * endGrowthMult() * penalty;
    spd += st.spd * absSpd * tempSpd * envSpd * spdGrowthMult() * penalty;
    spi += st.spi * absSpi * tempSpi * envSpi * spiGrowthMult() * penalty;

    // 成虫期额外 MOT 恢复
    if (stage == Stage::ADULT) {
        uint8_t motRec = FoodTypeInfo::adultMotRecovery(type);
        mot += motRec;
    }

    // 特殊记录
    if (stage == Stage::LARVA && type == FoodType::CITRUS) {
        larvaCitrusCount++;
    }
    if (stage == Stage::LARVA && type == FoodType::BERRY) {
        larvaBerryFed = true;
    }

    // Citrus 累计 3 次增加 SPI 上限 +0.3
    if (stage == Stage::LARVA && larvaCitrusCount == 3 && spiCapBonusTenths < 3) {
        spiCapBonusTenths = 3;
    }

    lastFeedTime = now;
    clampAttributes();
}

bool Bug::eatFromTray(uint64_t now, bool forceBite) {
    if (foodAmount == 0 || !foodInTray) return false;

    static constexpr uint32_t EAT_INTERVAL_MS = 2000;
    if (!forceBite && now - lastEatTime < EAT_INTERVAL_MS) return false;

    bool hungry = false;
    uint8_t eatChance = 0;
    if (stage == Stage::LARVA) {
        hungry = hunger < 95;
        eatChance = 90;
    } else if (stage == Stage::JUVENILE || stage == Stage::ADULT) {
        hungry = hunger < 80;
        eatChance = 70;
    } else {
        return false;
    }

    if (!forceBite && (!hungry || random(100) >= eatChance)) return false;

    foodAmount--;
    hunger += 6;
    lastEatTime = now;

    // 实际属性增益
    feed(trayFoodType, now);

    if (foodAmount == 0) {
        foodInTray = false;
        if (stage == Stage::LARVA) larvaFeeds++;
    }

    clampAttributes();
    return true;
}

void Bug::recordWoodRest(uint64_t now) {
    if (stage != Stage::ADULT || !woodPlaced || hunger < 50) {
        restStartTime = 0;
        woodRestAcc = 0;
        return;
    }

    if (restStartTime != 0 && now > restStartTime) {
        uint64_t delta = now - restStartTime;
        if (delta <= 10000ULL) {
            woodRestAcc += delta;
        } else {
            woodRestAcc = 0;
        }
    }
    restStartTime = now;

    static constexpr uint64_t WOOD_REST_END_GAIN_MS = 4ULL * 60 * 1000;
    float tempEnd = 1.0f;
    if (temperament == Temperament::RESILIENT) {
        tempEnd = 1.10f;
    } else if (temperament == Temperament::BRUTE) {
        tempEnd = 0.90f;
    }

    while (woodRestAcc >= WOOD_REST_END_GAIN_MS) {
        woodRestAcc -= WOOD_REST_END_GAIN_MS;
        end += 0.2f * endGrowthMult() * tempEnd;
    }
    clampAttributes();
}

bool Bug::poke(uint64_t now) {
    if (pokeAnger > 0 && now > lastPokeTime) {
        uint64_t elapsed = now - lastPokeTime;
        uint32_t decayTicks = (uint32_t)(elapsed / POKE_ANGER_DECAY_MS);
        uint32_t decay = decayTicks * POKE_ANGER_DECAY_VALUE;
        if (pokeAnger > decay) pokeAnger -= decay;
        else pokeAnger = 0;
    }

    if (now - lastPokeTime < POKE_COOLDOWN_MS) {
        Serial.printf("[Bug] poke in cooldown, remain %.1fs, anger=%d (anim only)\n",
                      (POKE_COOLDOWN_MS - (now - lastPokeTime)) / 1000.0f, pokeAnger);
        return true;
    }

    uint16_t newAnger = (uint16_t)pokeAnger + POKE_ANGER_PER_POKE;
    if (newAnger > 100) newAnger = 100;
    pokeAnger = (uint8_t)newAnger;

    uint32_t roll = random(100);
    Serial.printf("[Bug] poke anger=%d roll=%d\n", pokeAnger, roll);

    if (roll < pokeAnger) {
        lastPokeTime = now;
        pokeAnger = 0;

        motBuffAmount = POKE_MOT_BUFF;
        motBuffEndTime = now + MOT_BUFF_DURATION_MS;
        mot += motBuffAmount;
        if (mot > 100) mot = 100;
        if (hunger < 30 && mot > 50) mot = 50;

        Serial.printf("[Bug] poke triggered! MOT+%d buff until %llu (now=%llu)\n",
                      motBuffAmount, motBuffEndTime, now);
        return true;
    }

    Serial.printf("[Bug] poke missed, anger=%d\n", pokeAnger);
    return true;
}

bool Bug::onShake(uint64_t now) {
    switch (stage) {
        case Stage::EGG:
            if (eggShakeDelayAcc >= EGG_SHAKE_DELAY_MAX_MS) return false;
            {
                uint32_t delay = EGG_SHAKE_DELAY_MS;
                uint32_t remaining = EGG_SHAKE_DELAY_MAX_MS - eggShakeDelayAcc;
                if (delay > remaining) delay = remaining;
                stageStartTime += delay;
                eggShakeDelayAcc += delay;
            }
            onEggShake(now, false);
            return true;

        case Stage::LARVA:
            spi -= 0.2f;
            clampAttributes();
            return true;

        case Stage::PUPA:
            pupaShakes++;
            spi -= 1.0f;
            clampAttributes();
            return true;

        case Stage::ADULT:
            if (now - lastShakeTrainTime < 3ULL * 60 * 1000) return false;
            lastShakeTrainTime = now;
            str += 0.1f;
            mot += 20;
            clampAttributes();
            return true;

        default:
            return false;
    }
}

void Bug::onWater(uint64_t now) {
    (void)now;
    // 喷水目前只在卵期有计数意义
}

void Bug::onEggPoke(uint64_t now) {
    (void)now;
    eggPokeCount++;
    eggLastAction = (uint8_t)EggAction::POKE;
}

void Bug::onEggWater(uint64_t now) {
    (void)now;
    eggWaterCount++;
    eggLastAction = (uint8_t)EggAction::WATER;
}

void Bug::onEggShake(uint64_t now, bool violent) {
    (void)now;
    eggShakeCount++;
    if (violent) eggViolentShakeCount++;
    eggLastAction = (uint8_t)EggAction::SHAKE;
}

void Bug::onEggTilt(uint64_t now, uint32_t deltaMs, bool left) {
    (void)now;
    if (left) eggLeftTiltMs += deltaMs;
    else eggRightTiltMs += deltaMs;
}

float Bug::onBattleEnd(bool win, uint64_t now, float spiReward) {
    (void)now;
    float beforeSpi = spi;
    if (win) {
        wins++;
        if ((int)foodCounts[(uint8_t)FoodType::DROP] + 2 <= MAX_FOOD_COUNT) foodCounts[(uint8_t)FoodType::DROP] += 2;
        else foodCounts[(uint8_t)FoodType::DROP] = MAX_FOOD_COUNT;
    } else {
        losses++;
    }
    if (spiReward < 0.0f) spiReward = win ? 0.5f : 0.2f;
    if (spiReward > 0.0f) spi += spiReward;
    mot = 50;
    clampAttributes();
    return spi - beforeSpi;
}

uint8_t Bug::getTotalFoodCount() const {
    uint8_t total = 0;
    for (int i = 0; i < (int)FoodType::COUNT; i++) total += foodCounts[i];
    return total;
}

void Bug::addFood(FoodType type, uint8_t amount) {
    uint16_t sum = foodCounts[(uint8_t)type] + amount;
    if (sum > MAX_FOOD_COUNT) sum = MAX_FOOD_COUNT;
    foodCounts[(uint8_t)type] = (uint8_t)sum;
}

void Bug::removeFood(FoodType type, uint8_t amount) {
    if (foodCounts[(uint8_t)type] >= amount) {
        foodCounts[(uint8_t)type] -= amount;
    } else {
        foodCounts[(uint8_t)type] = 0;
    }
}

void Bug::addRottenWood(uint8_t amount) {
    uint16_t sum = rottenWood + amount;
    if (sum > 99) sum = 99;
    rottenWood = (uint8_t)sum;
}

bool Bug::placeWood() {
    if (rottenWood == 0 || woodPlaced) return false;
    rottenWood--;
    woodPlaced = true;
    return true;
}

uint8_t Bug::getPaletteId() const {
    uint8_t appDom = dominant(geneAPP);
    if (appDom < 4) return 0;
    if (appDom < 8) return 1;
    if (appDom < 12) return 2;
    return 3;
}

const char* Bug::getHatchHint() const {
    uint16_t total = dominant(geneVIG) + dominant(geneATK) + dominant(geneMNT);
    if (total >= 30) return UiStrings::HATCH_HINT_VIGOROUS;
    if (total >= 20) return UiStrings::HATCH_HINT_HEALTHY;
    if (total >= 10) return UiStrings::HATCH_HINT_QUIET;
    return UiStrings::HATCH_HINT_WEAK;
}

void Bug::resetAfterDeath(uint64_t now) {
    uint8_t prevGen = generation;
    initNew(now);
    generation = prevGen + 1;
}

void Bug::release(uint64_t now) {
    // 保留基因并小幅变异
    auto mutate = [](uint8_t g) -> uint8_t {
        if (random(100) < 5) {
            int d = (g >> 4) + random(-2, 3);
            int r = (g & 0x0F) + random(-2, 3);
            if (d < 0) d = 0; if (d > 15) d = 15;
            if (r < 0) r = 0; if (r > 15) r = 15;
            return (uint8_t)((d << 4) | r);
        }
        return g;
    };
    geneVIG = mutate(geneVIG);
    geneATK = mutate(geneATK);
    geneMNT = mutate(geneMNT);
    geneEND = mutate(geneEND);
    geneAPP = mutate(geneAPP);

    uint8_t prevGen = generation;
    initNew(now);
    generation = prevGen + 1;
    releaseCountTotal++; // 新虫继承累计放生次数（文档为累计纪念）
}

void Bug::addReleaseCount() {
    if (releaseCountTotal < 255) releaseCountTotal++;
}

void Bug::recordCupParticipation() {
    cupParticipated++;
}

void Bug::recordCupResult(uint8_t rank) {
    if (cupBest == 0 || rank < cupBest) cupBest = rank;
}

void Bug::recordCupWin() {
    if (cupWins < 255) cupWins++;
}

void Bug::recordCupLegendKill() {
    if (cupLegendKills < 255) cupLegendKills++;
}

void Bug::setAchievementFlag(uint16_t flag) {
    achievementFlags |= flag;
}

bool Bug::hasAchievementFlag(uint16_t flag) const {
    return (achievementFlags & flag) != 0;
}

void Bug::resetCupStreak() {
    cupStreak = 0;
}

void Bug::incrementCupStreak() {
    if (cupStreak < 255) cupStreak++;
}

// ---------- 存档格式 ----------
// v8：新增探索/杯赛字段（releaseCountTotal, cupParticipated, cupBest, cupWins, cupLegendKills, achievementFlags, cupStreak）
static constexpr uint8_t SAVE_VERSION = 8;

void Bug::save(uint8_t* buf, uint16_t& len) const {
    struct SaveData {
        uint8_t version;
        uint8_t geneVIG, geneATK, geneMNT, geneEND, geneAPP;
        uint8_t siz, str, end, spd, spi;
        uint8_t mot, hunger;
        uint8_t stage, alive;
        uint8_t foodCounts[6];
        uint8_t rottenWood, woodPlaced;
        uint8_t foodInTray, trayFoodType, foodAmount;
        uint32_t stageStart;
        uint32_t lastFeed;
        uint32_t lastSap;
        uint32_t lastShake;
        uint32_t eggShakeDelay;
        uint32_t restStart;
        uint32_t lastUpdate;
        uint32_t lastEat;
        uint8_t larvaFeeds, pupaShakes;
        uint8_t wins, losses;
        uint8_t generation;
        uint32_t motBuffEnd;
        uint8_t motBuffAmount;
        uint8_t pokeAnger;
        uint8_t temperament;
        uint32_t eggStart;
        uint8_t eggShakeCount;
        uint8_t eggViolentShakeCount;
        uint8_t eggPokeCount;
        uint8_t eggWaterCount;
        uint32_t eggLeftTiltMs;
        uint32_t eggRightTiltMs;
        uint8_t eggLastAction;
        uint8_t foodTrayLevel;
        uint8_t foodTrayType;
        uint8_t woodStyle;
        uint8_t larvaCitrusCount;
        uint8_t larvaBerryFed;
        uint8_t spiCapBonusTenths;
        uint8_t releaseCountTotal;
        uint16_t cupParticipated;
        uint8_t cupBest;
        uint8_t cupWins;
        uint8_t cupLegendKills;
        uint16_t achievementFlags;
        uint8_t cupStreak;
        uint8_t reserved[1];
    } __attribute__((packed));

    SaveData sd = {};
    sd.version = SAVE_VERSION;
    sd.geneVIG = geneVIG; sd.geneATK = geneATK; sd.geneMNT = geneMNT; sd.geneEND = geneEND; sd.geneAPP = geneAPP;
    sd.siz = (uint8_t)roundf(siz);
    sd.str = (uint8_t)roundf(str);
    sd.end = (uint8_t)roundf(end);
    sd.spd = (uint8_t)roundf(spd);
    sd.spi = (uint8_t)roundf(spi);
    sd.mot = mot;
    sd.hunger = hunger;
    sd.stage = (uint8_t)stage;
    sd.alive = alive ? 1 : 0;
    for (int i = 0; i < 6; i++) sd.foodCounts[i] = foodCounts[i];
    sd.rottenWood = rottenWood;
    sd.woodPlaced = woodPlaced ? 1 : 0;
    sd.foodInTray = foodInTray ? 1 : 0;
    sd.trayFoodType = (uint8_t)trayFoodType;
    sd.foodAmount = foodAmount;
    sd.stageStart = (uint32_t)(stageStartTime / 1000ULL);
    sd.lastFeed = (uint32_t)(lastFeedTime / 1000ULL);
    sd.lastSap = (uint32_t)(lastSapProduceTime / 1000ULL);
    sd.lastShake = (uint32_t)(lastShakeTrainTime / 1000ULL);
    sd.eggShakeDelay = eggShakeDelayAcc / 1000;
    sd.restStart = (uint32_t)(restStartTime / 1000ULL);
    sd.lastUpdate = (uint32_t)(lastUpdateTime / 1000ULL);
    sd.lastEat = (uint32_t)(lastEatTime / 1000ULL);
    sd.larvaFeeds = larvaFeeds;
    sd.pupaShakes = pupaShakes;
    sd.wins = wins;
    sd.losses = losses;
    sd.generation = generation;
    sd.motBuffEnd = (uint32_t)(motBuffEndTime / 1000ULL);
    sd.motBuffAmount = motBuffAmount;
    sd.pokeAnger = pokeAnger;
    sd.temperament = (uint8_t)temperament;
    sd.eggStart = (uint32_t)(eggStartTime / 1000ULL);
    sd.eggShakeCount = eggShakeCount;
    sd.eggViolentShakeCount = eggViolentShakeCount;
    sd.eggPokeCount = eggPokeCount;
    sd.eggWaterCount = eggWaterCount;
    sd.eggLeftTiltMs = eggLeftTiltMs;
    sd.eggRightTiltMs = eggRightTiltMs;
    sd.eggLastAction = eggLastAction;
    sd.foodTrayLevel = foodTrayLevel;
    sd.foodTrayType = (uint8_t)foodTrayType;
    sd.woodStyle = woodStyle;
    sd.larvaCitrusCount = larvaCitrusCount;
    sd.larvaBerryFed = larvaBerryFed ? 1 : 0;
    sd.spiCapBonusTenths = spiCapBonusTenths;
    sd.releaseCountTotal = releaseCountTotal;
    sd.cupParticipated = cupParticipated;
    sd.cupBest = cupBest;
    sd.cupWins = cupWins;
    sd.cupLegendKills = cupLegendKills;
    sd.achievementFlags = achievementFlags;
    sd.cupStreak = cupStreak;

    memcpy(buf, &sd, sizeof(sd));
    len = sizeof(sd);
}


bool Bug::load(const uint8_t* buf, uint16_t len) {
    struct SaveDataV8 {
        uint8_t version;
        uint8_t geneVIG, geneATK, geneMNT, geneEND, geneAPP;
        uint8_t siz, str, end, spd, spi;
        uint8_t mot, hunger;
        uint8_t stage, alive;
        uint8_t foodCounts[6];
        uint8_t rottenWood, woodPlaced;
        uint8_t foodInTray, trayFoodType, foodAmount;
        uint32_t stageStart;
        uint32_t lastFeed;
        uint32_t lastSap;
        uint32_t lastShake;
        uint32_t eggShakeDelay;
        uint32_t restStart;
        uint32_t lastUpdate;
        uint32_t lastEat;
        uint8_t larvaFeeds, pupaShakes;
        uint8_t wins, losses;
        uint8_t generation;
        uint32_t motBuffEnd;
        uint8_t motBuffAmount;
        uint8_t pokeAnger;
        uint8_t temperament;
        uint32_t eggStart;
        uint8_t eggShakeCount;
        uint8_t eggViolentShakeCount;
        uint8_t eggPokeCount;
        uint8_t eggWaterCount;
        uint32_t eggLeftTiltMs;
        uint32_t eggRightTiltMs;
        uint8_t eggLastAction;
        uint8_t foodTrayLevel;
        uint8_t foodTrayType;
        uint8_t woodStyle;
        uint8_t larvaCitrusCount;
        uint8_t larvaBerryFed;
        uint8_t spiCapBonusTenths;
        uint8_t releaseCountTotal;
        uint16_t cupParticipated;
        uint8_t cupBest;
        uint8_t cupWins;
        uint8_t cupLegendKills;
        uint16_t achievementFlags;
        uint8_t cupStreak;
        uint8_t reserved[1];
    } __attribute__((packed));

    struct SaveDataV7 {
        uint8_t version;
        uint8_t geneVIG, geneATK, geneMNT, geneEND, geneAPP;
        uint8_t siz, str, end, spd, spi;
        uint8_t mot, hunger;
        uint8_t stage, alive;
        uint8_t foodCounts[6];
        uint8_t rottenWood, woodPlaced;
        uint8_t foodInTray, trayFoodType, foodAmount;
        uint32_t stageStart;
        uint32_t lastFeed;
        uint32_t lastSap;
        uint32_t lastShake;
        uint32_t eggShakeDelay;
        uint32_t restStart;
        uint32_t lastUpdate;
        uint32_t lastEat;
        uint8_t larvaFeeds, pupaShakes;
        uint8_t wins, losses;
        uint8_t generation;
        uint32_t motBuffEnd;
        uint8_t motBuffAmount;
        uint8_t pokeAnger;
        uint8_t temperament;
        uint32_t eggStart;
        uint8_t eggShakeCount;
        uint8_t eggViolentShakeCount;
        uint8_t eggPokeCount;
        uint8_t eggWaterCount;
        uint32_t eggLeftTiltMs;
        uint32_t eggRightTiltMs;
        uint8_t eggLastAction;
        uint8_t foodTrayLevel;
        uint8_t foodTrayType;
        uint8_t woodStyle;
        uint8_t larvaCitrusCount;
        uint8_t larvaBerryFed;
        uint8_t spiCapBonusTenths;
        uint8_t reserved[1];
    } __attribute__((packed));

    struct SaveDataV6 {
        uint8_t version;
        uint8_t geneVIG, geneATK, geneMNT, geneAPP;
        uint8_t siz, str, end, spd, spi;
        uint8_t mot, hunger;
        uint8_t stage, alive;
        uint8_t foodCounts[6];
        uint8_t rottenWood, woodPlaced;
        uint8_t foodInTray, trayFoodType, foodAmount;
        uint32_t stageStart;
        uint32_t lastFeed;
        uint32_t lastSap;
        uint32_t lastShake;
        uint32_t eggShakeDelay;
        uint32_t restStart;
        uint32_t lastUpdate;
        uint32_t lastEat;
        uint8_t larvaFeeds, pupaShakes;
        uint8_t wins, losses;
        uint8_t generation;
        uint32_t motBuffEnd;
        uint8_t motBuffAmount;
        uint8_t pokeAnger;
        uint8_t temperament;
        uint32_t eggStart;
        uint8_t eggShakeCount;
        uint8_t eggViolentShakeCount;
        uint8_t eggPokeCount;
        uint8_t eggWaterCount;
        uint32_t eggLeftTiltMs;
        uint32_t eggRightTiltMs;
        uint8_t eggLastAction;
        uint8_t foodTrayLevel;
        uint8_t foodTrayType;
        uint8_t woodStyle;
        uint8_t larvaCitrusCount;
        uint8_t larvaBerryFed;
        uint8_t spiCapBonusTenths;
        uint8_t reserved[1];
    } __attribute__((packed));

    struct SaveDataV5 {
        uint8_t version;
        uint8_t geneVIG, geneATK, geneMNT, geneAPP;
        uint8_t siz, str, end, spi;
        uint8_t mot, hunger;
        uint8_t stage, alive;
        uint8_t sap, rottenWood, woodPlaced;
        uint8_t foodInTray, foodAmount;
        uint32_t stageStart;
        uint32_t lastFeed;
        uint32_t lastSap;
        uint32_t lastShake;
        uint32_t eggShakeDelay;
        uint32_t restStart;
        uint32_t lastUpdate;
        uint32_t lastEat;
        uint8_t larvaFeeds, pupaShakes;
        uint8_t wins, losses;
        uint8_t generation;
        uint32_t motBuffEnd;
        uint8_t motBuffAmount;
        uint8_t pokeAnger;
        uint8_t reserved[2];
    } __attribute__((packed));

    auto applyCommon = [this](const SaveDataV8& sd) {
        geneVIG = sd.geneVIG; geneATK = sd.geneATK; geneMNT = sd.geneMNT; geneEND = sd.geneEND; geneAPP = sd.geneAPP;
        siz = sd.siz; str = sd.str; end = sd.end; spd = sd.spd; spi = sd.spi;
        mot = sd.mot; hunger = sd.hunger;
        stage = (Stage)sd.stage;
        alive = sd.alive != 0;
        for (int i = 0; i < 6; i++) foodCounts[i] = sd.foodCounts[i];
        rottenWood = sd.rottenWood;
        woodPlaced = sd.woodPlaced != 0;
        foodInTray = sd.foodInTray != 0;
        trayFoodType = (FoodType)sd.trayFoodType;
        foodAmount = sd.foodAmount;
        stageStartTime = (uint64_t)sd.stageStart * 1000ULL;
        lastFeedTime = (uint64_t)sd.lastFeed * 1000ULL;
        lastSapProduceTime = (uint64_t)sd.lastSap * 1000ULL;
        lastShakeTrainTime = (uint64_t)sd.lastShake * 1000ULL;
        eggShakeDelayAcc = sd.eggShakeDelay * 1000U;
        restStartTime = (uint64_t)sd.restStart * 1000ULL;
        woodRestAcc = 0;
        lastUpdateTime = (uint64_t)sd.lastUpdate * 1000ULL;
        lastEatTime = (uint64_t)sd.lastEat * 1000ULL;
        larvaFeeds = sd.larvaFeeds;
        pupaShakes = sd.pupaShakes;
        wins = sd.wins;
        losses = sd.losses;
        generation = sd.generation;
        motBuffEndTime = (uint64_t)sd.motBuffEnd * 1000ULL;
        motBuffAmount = sd.motBuffAmount;
        pokeAnger = sd.pokeAnger;
        temperament = (Temperament)sd.temperament;
        eggStartTime = (uint64_t)sd.eggStart * 1000ULL;
        eggShakeCount = sd.eggShakeCount;
        eggViolentShakeCount = sd.eggViolentShakeCount;
        eggPokeCount = sd.eggPokeCount;
        eggWaterCount = sd.eggWaterCount;
        eggLeftTiltMs = sd.eggLeftTiltMs;
        eggRightTiltMs = sd.eggRightTiltMs;
        eggLastAction = sd.eggLastAction;
        foodTrayLevel = sd.foodTrayLevel;
        foodTrayType = (FoodType)sd.foodTrayType;
        woodStyle = sd.woodStyle;
        larvaCitrusCount = sd.larvaCitrusCount;
        larvaBerryFed = sd.larvaBerryFed != 0;
        spiCapBonusTenths = sd.spiCapBonusTenths;
        releaseCountTotal = sd.releaseCountTotal;
        cupParticipated = sd.cupParticipated;
        cupBest = sd.cupBest;
        cupWins = sd.cupWins;
        cupLegendKills = sd.cupLegendKills;
        achievementFlags = sd.achievementFlags;
        cupStreak = sd.cupStreak;
        clampAttributes();
    };

    if (len < 1) return false;
    uint8_t ver = buf[0];

    if (ver == SAVE_VERSION && len >= sizeof(SaveDataV8)) {
        applyCommon(*reinterpret_cast<const SaveDataV8*>(buf));
        return true;
    }

    if (ver == 7 && len >= sizeof(SaveDataV7)) {
        const SaveDataV7& sd = *reinterpret_cast<const SaveDataV7*>(buf);
        SaveDataV8 tmp = {};
        memcpy(&tmp, &sd, sizeof(SaveDataV7));
        tmp.version = SAVE_VERSION;
        // v7 没有探索/杯赛字段，保持默认 0
        tmp.releaseCountTotal = 0;
        tmp.cupParticipated = 0;
        tmp.cupBest = 0;
        tmp.cupWins = 0;
        tmp.cupLegendKills = 0;
        tmp.achievementFlags = 0;
        tmp.cupStreak = 0;
        applyCommon(tmp);
        return true;
    }

    if (ver == 6 && len >= sizeof(SaveDataV6)) {
        const SaveDataV6& sd = *reinterpret_cast<const SaveDataV6*>(buf);
        SaveDataV8 tmp = {};
        memcpy(&tmp, &sd, sizeof(SaveDataV6));
        tmp.version = SAVE_VERSION;
        tmp.geneEND = 0x55;
        applyCommon(tmp);
        return true;
    }

    if (ver == 5 && len >= sizeof(SaveDataV5)) {
        const SaveDataV5& sd = *reinterpret_cast<const SaveDataV5*>(buf);
        SaveDataV8 tmp = {};
        tmp.version = SAVE_VERSION;
        tmp.geneVIG = sd.geneVIG; tmp.geneATK = sd.geneATK; tmp.geneMNT = sd.geneMNT; tmp.geneAPP = sd.geneAPP;
        tmp.siz = sd.siz; tmp.str = sd.str; tmp.end = sd.end; tmp.spi = sd.spi;
        tmp.mot = sd.mot; tmp.hunger = sd.hunger;
        tmp.stage = sd.stage;
        if (tmp.stage == 3) tmp.stage = (uint8_t)Stage::ADULT;
        tmp.alive = sd.alive;
        tmp.foodCounts[0] = sd.sap;
        tmp.rottenWood = sd.rottenWood;
        tmp.woodPlaced = sd.woodPlaced;
        tmp.foodInTray = sd.foodInTray;
        tmp.foodAmount = sd.foodAmount;
        tmp.stageStart = sd.stageStart;
        tmp.lastFeed = sd.lastFeed;
        tmp.lastSap = sd.lastSap;
        tmp.lastShake = sd.lastShake;
        tmp.eggShakeDelay = sd.eggShakeDelay;
        tmp.restStart = sd.restStart;
        tmp.lastUpdate = sd.lastUpdate;
        tmp.lastEat = sd.lastEat;
        tmp.larvaFeeds = sd.larvaFeeds;
        tmp.pupaShakes = sd.pupaShakes;
        tmp.wins = sd.wins;
        tmp.losses = sd.losses;
        tmp.generation = sd.generation;
        tmp.motBuffEnd = sd.motBuffEnd;
        tmp.motBuffAmount = sd.motBuffAmount;
        tmp.pokeAnger = sd.pokeAnger;
        tmp.temperament = (uint8_t)Temperament::SPIRIT;
        tmp.eggStart = sd.stageStart;
        tmp.foodTrayLevel = 1;
        tmp.foodTrayType = (uint8_t)FoodType::DROP;
        tmp.woodStyle = 0;
        tmp.geneEND = 0x55;
        applyCommon(tmp);
        return true;
    }

    return false;
}
