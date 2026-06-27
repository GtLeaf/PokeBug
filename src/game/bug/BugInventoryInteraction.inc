void Bug::setFoodTray(uint8_t level, FoodType type) {
    foodTrayLevel = level;
    foodTrayType = type;
    if (foodTrayLevel == 0) {
        foodInTray = false;
        foodAmount = 0;
    }
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
            absSiz = 0.15f; absStr = 0.15f; absEnd = 0.15f; absSpd = 0.15f; absSpi = 0.15f;
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
    if (hunger >= 100) return false;

    static constexpr uint32_t EAT_INTERVAL_MS = 2000;
    if (!forceBite && now - lastEatTime < EAT_INTERVAL_MS) return false;

    bool hungry = false;
    uint8_t eatChance = 0;
    if (stage == Stage::LARVA) {
        hungry = hunger < 95;
        eatChance = 90;
    } else if (stage == Stage::JUVENILE || stage == Stage::ADULT) {
        if (hunger >= 80) return false;
        hungry = hunger < 80;
        eatChance = 70;
    } else {
        return false;
    }

    if (!forceBite && (!hungry || random(100) >= eatChance)) return false;

    foodAmount--;
    modHunger((int8_t)FoodTypeInfo::hungerPerBite(trayFoodType));
    if (stage == Stage::LARVA) recordLarvaFeedAmount();
    deathTimerStart = 0;
    lastEatTime = now;

    // 实际属性增益
    feed(trayFoodType, now);

    if (foodAmount == 0) {
        foodInTray = false;
    }

    clampAttributes();
    return true;
}

bool Bug::consumeTrayBiteForVisitor(uint8_t& hungerGain, FoodType& foodType) {
    hungerGain = 0;
    foodType = trayFoodType;
    if (foodAmount == 0 || !foodInTray) return false;

    foodAmount--;
    hungerGain = FoodTypeInfo::hungerPerBite(trayFoodType);
    foodType = trayFoodType;
    if (foodAmount == 0) {
        foodInTray = false;
    }
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

bool Bug::isWoodUnlocked(uint8_t style) const {
    if (style >= WoodTypeInfo::COUNT) return false;
    return woodUnlocked[style] != 0;
}

uint8_t Bug::getWoodCount(uint8_t style) const {
    return isWoodUnlocked(style) ? 1 : 0;
}

uint8_t Bug::getTotalWoodCount() const {
    uint16_t total = 0;
    for (int i = 0; i < WoodTypeInfo::COUNT; i++) {
        if (woodUnlocked[i]) total++;
    }
    return total > 255 ? 255 : (uint8_t)total;
}

void Bug::addWood(uint8_t style, uint8_t amount) {
    if (amount == 0) return;
    if (style >= WoodTypeInfo::COUNT) style = 0;
    woodUnlocked[style] = 1;
}

void Bug::removeWoodItem(uint8_t style, uint8_t amount) {
    if (amount == 0) return;
    if (style >= WoodTypeInfo::COUNT) return;
    woodUnlocked[style] = 0;
    if (woodStyle == style) woodPlaced = false;
}

uint8_t Bug::getItemCount(ItemId id) const {
    switch (ItemCatalog::kind(id)) {
        case ItemKind::FOOD:
            return getFoodCount((FoodType)ItemCatalog::index(id));
        case ItemKind::WOOD:
            return getWoodCount(ItemCatalog::index(id));
        default:
            return 0;
    }
}

bool Bug::addItem(ItemId id, uint8_t amount) {
    if (amount == 0 || !ItemCatalog::isExchangeable(id)) return false;
    switch (ItemCatalog::kind(id)) {
        case ItemKind::FOOD:
            addFood((FoodType)ItemCatalog::index(id), amount);
            return true;
        case ItemKind::WOOD:
            addWood(ItemCatalog::index(id), amount);
            return true;
        default:
            return false;
    }
}

bool Bug::removeItem(ItemId id, uint8_t amount) {
    if (amount == 0 || getItemCount(id) < amount) return false;
    switch (ItemCatalog::kind(id)) {
        case ItemKind::FOOD:
            removeFood((FoodType)ItemCatalog::index(id), amount);
            return true;
        case ItemKind::WOOD:
            removeWoodItem(ItemCatalog::index(id), amount);
            return true;
        default:
            return false;
    }
}

void Bug::addRottenWood(uint8_t amount) {
    addWood((uint8_t)WoodType::TWIG, amount);
}

bool Bug::placeWood() {
    if (woodStyle >= WoodTypeInfo::COUNT) woodStyle = 0;
    if (!isWoodUnlocked(woodStyle) || woodPlaced) return false;
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
