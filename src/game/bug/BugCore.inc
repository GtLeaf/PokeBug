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
    for (int i = 0; i < WoodTypeInfo::COUNT; i++) woodUnlocked[i] = 0;
    woodPlaced = false;
    sleeping = false;
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

void Bug::ensureMinHunger(uint8_t minHunger) {
    if (hunger < minHunger) {
        hunger = minHunger;
        deathTimerStart = 0;  // 清除饥饿死亡计时，避免睡觉时饿死
    }
}

void Bug::skipSimulationTime(uint64_t deltaMs) {
    if (deltaMs == 0) return;

    stageStartTime += deltaMs;
    eggStartTime += deltaMs;
    lastUpdateTime += deltaMs;
    if (lastFeedTime != 0) lastFeedTime += deltaMs;
    if (lastSapProduceTime != 0) lastSapProduceTime += deltaMs;
    if (lastShakeTrainTime != 0) lastShakeTrainTime += deltaMs;
    if (restStartTime != 0) restStartTime += deltaMs;
    if (lastPokeTime != 0) lastPokeTime += deltaMs;
    if (motBuffEndTime != 0) motBuffEndTime += deltaMs;
    if (lastEatTime != 0) lastEatTime += deltaMs;
    if (deathTimerStart != 0) deathTimerStart += deltaMs;
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

    // 睡眠快进/深睡更新中，如果已经饿到危险区且盘中有食物，允许自动咬食。
    static constexpr uint8_t SLEEP_AUTO_EAT_HUNGER = 30;
    if (sleeping && stage != Stage::LARVA && hunger < SLEEP_AUTO_EAT_HUNGER) {
        eatFromTray(now, true);
    } else if (stage == Stage::JUVENILE) {
        // 青年期自主进食；成虫平时由 TerrariumScene 走到食物盘后控制进食。
        eatFromTray(now);
    }

    // 阶段推进
    checkStageTransition(now);
    clampAttributes();
}

void Bug::updateHunger(uint64_t now, uint32_t deltaMs) {
    if (stage == Stage::PUPA) {
        uint64_t pupaEnd = stageStartTime + PUPA_DURATION_MS;
        if (hunger > PUPA_TARGET_HUNGER) {
            if (now >= pupaEnd) {
                hunger = PUPA_TARGET_HUNGER;
                hungerDropAcc = 0;
            } else {
                hungerDropAcc += deltaMs;
                while (hunger > PUPA_TARGET_HUNGER) {
                    uint64_t remainingMs = pupaEnd - now;
                    uint8_t remainingDrops = hunger - PUPA_TARGET_HUNGER;
                    uint64_t dropMs = remainingMs / remainingDrops;
                    if (dropMs == 0) dropMs = 1;
                    if (hungerDropAcc < dropMs) break;
                    hungerDropAcc -= dropMs;
                    hunger--;
                }
            }
        } else {
            hungerDropAcc = 0;
        }
        deathTimerStart = 0;
        if (hunger < 30 && mot > 50) mot = 50;
        return;
    }

    hungerDropAcc += deltaMs;
    uint32_t baseDropMs = stage == Stage::LARVA ? LARVA_HUNGER_DROP_MS : HUNGER_DROP_MS;
    uint32_t dropMs = sleeping ? baseDropMs * 3 : baseDropMs;
    while (hungerDropAcc >= dropMs) {
        hungerDropAcc -= dropMs;
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

bool Bug::eatSubstrateBite(uint64_t now, uint8_t hungerGain) {
    if (stage != Stage::LARVA) return false;
    if (hunger >= 100) return false;

    modHunger((int8_t)hungerGain);
    recordLarvaFeedAmount();
    deathTimerStart = 0;
    lastEatTime = now;
    clampAttributes();
    return true;
}

void Bug::recordLarvaFeedAmount(uint8_t amount) {
    if (stage != Stage::LARVA || amount == 0) return;
    uint16_t total = (uint16_t)larvaFeeds + amount;
    larvaFeeds = (uint8_t)(total > 255 ? 255 : total);
}

uint8_t Bug::getLarvaAgeIndex(uint64_t now) const {
    if (stage != Stage::LARVA) return 0;
    uint64_t elapsed = now > stageStartTime ? now - stageStartTime : 0;
    if (elapsed >= LARVA_AGE1_DURATION_MS + LARVA_AGE2_DURATION_MS &&
        larvaFeeds >= LARVA_FEED_TO_AGE3) {
        return 2;
    }
    if (elapsed >= LARVA_AGE1_DURATION_MS &&
        larvaFeeds >= LARVA_FEED_TO_AGE2) {
        return 1;
    }
    return 0;
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
            return elapsed >= LARVA_DURATION_MS && larvaFeeds >= LARVA_FEED_TO_PUPA;
        case Stage::PUPA:
            return elapsed >= PUPA_DURATION_MS;
        case Stage::JUVENILE:
            return elapsed >= JUVENILE_DURATION_MS;
        default:
            return false;
    }
}

float Bug::getStageProgress(uint64_t now) const {
    uint64_t duration = 0;
    switch (stage) {
        case Stage::EGG: duration = EGG_DURATION_MS; break;
        case Stage::LARVA: duration = LARVA_DURATION_MS; break;
        case Stage::PUPA: duration = PUPA_DURATION_MS; break;
        case Stage::JUVENILE: duration = JUVENILE_DURATION_MS; break;
        case Stage::ADULT:
        default:
            return 1.0f;
    }
    if (duration == 0 || now <= stageStartTime) return 0.0f;
    float progress = (float)(now - stageStartTime) / (float)duration;
    if (progress < 0.0f) return 0.0f;
    if (progress > 1.0f) return 1.0f;
    return progress;
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
            larvaFeeds = 0;
            // 孵化时给初始食物
            foodCounts[(uint8_t)FoodType::DROP] = 6;
            break;
        case Stage::LARVA:
            stageStartTime += LARVA_DURATION_MS;
            stage = Stage::PUPA;
            str += 1.0f;
            siz += 0.5f;
            addRottenWood(1);
            break;
        case Stage::PUPA:
            stageStartTime += PUPA_DURATION_MS;
            stage = Stage::JUVENILE;
            hunger = PUPA_TARGET_HUNGER;
            hungerDropAcc = 0;
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

void Bug::debugSetStage(Stage nextStage, uint64_t now) {
    stage = nextStage;
    alive = true;
    stageStartTime = now;
    lastUpdateTime = now;
    deathTimerStart = 0;
    hungerDropAcc = 0;
    lastEatTime = 0;
    sleeping = false;
    pupaShakes = 0;
    if (stage == Stage::EGG) {
        eggStartTime = now;
        larvaFeeds = 0;
    } else if (stage == Stage::LARVA) {
        larvaFeeds = 0;
    }
    clampAttributes();
}

void Bug::debugSetTemperament(Temperament nextTemperament) {
    if ((uint8_t)nextTemperament >= 6) return;
    temperament = nextTemperament;
    clampAttributes();
}

float Bug::debugGetAttr(uint8_t index) const {
    switch (index) {
        case 0: return siz;
        case 1: return str;
        case 2: return end;
        case 3: return spd;
        case 4: return spi;
        default: return 1.0f;
    }
}

uint8_t Bug::debugGetAttrCap(uint8_t index) const {
    switch (index) {
        case 0: return sizCap();
        case 1: return strCap();
        case 2: return endCap();
        case 3: return spdCap();
        case 4: return spiCap();
        default: return 1;
    }
}

void Bug::debugSetAttr(uint8_t index, float value) {
    if (value < 1.0f) value = 1.0f;
    uint8_t cap = debugGetAttrCap(index);
    if (value > cap) value = (float)cap;
    switch (index) {
        case 0: siz = value; break;
        case 1: str = value; break;
        case 2: end = value; break;
        case 3: spd = value; break;
        case 4: spi = value; break;
        default: return;
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
        default: return UiStrings::UNKNOWN_SHORT;
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
    if (foodTrayLevel > 0) {
        if (foodTrayLevel == 2) mult *= 1.03f;
        else if (foodTrayLevel == 3) mult *= 1.06f;

        // 食物倾向加成
        int foodAttr = FoodTypeInfo::envAttribute(foodTrayType);
        if (foodAttr == attrIndex) mult *= 1.03f;
    }

    // 腐木加成（仅成虫）
    if (stage == Stage::ADULT && woodPlaced) {
        int woodAttr = WoodTypeInfo::envAttribute((WoodType)woodStyle);
        if (woodAttr == attrIndex) mult *= 1.03f;
    }

    return mult;
}

