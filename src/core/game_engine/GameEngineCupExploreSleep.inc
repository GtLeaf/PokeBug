void GameEngine::checkCupCycle() {
    // 深睡等场景可能一次性推进多个 7 天周期，用循环对齐到当前周期
    while (gameNow >= (uint64_t)lastCupGameTime * 1000ULL + CUP_CYCLE_MS) {
        lastCupGameTime += (uint32_t)(CUP_CYCLE_MS / 1000ULL);
        cupSeason++;
        cupCycleState = CupCycleState::IDLE;
        Serial.printf("[Engine] Cup season advanced to %u, startTime=%u\n", cupSeason, lastCupGameTime);
    }

    uint64_t registerStart = (uint64_t)lastCupGameTime * 1000ULL;
    uint64_t registerEnd   = registerStart + CUP_REGISTER_MS;

    switch (cupCycleState) {
        case CupCycleState::IDLE:
            if (gameNow >= registerEnd) {
                // 已过报名截止时间，本赛季自动关闭
                cupCycleState = CupCycleState::REGISTER_EXPIRED;
                Serial.printf("[Engine] Cup registration expired (reboot gap)\n");
            } else if (gameNow >= registerStart) {
                // 报名期开始
                cupCycleState = CupCycleState::REGISTER_OPEN;
                Serial.printf("[Engine] Cup registration open until gameNow=%llu\n", registerEnd);
            }
            break;

        case CupCycleState::REGISTER_OPEN:
            if (gameNow >= registerEnd) {
                // 报名结束，玩家未参与
                cupCycleState = CupCycleState::REGISTER_EXPIRED;
                Serial.printf("[Engine] Cup registration expired\n");
            }
            break;

        case CupCycleState::REGISTER_EXPIRED:
        case CupCycleState::IN_PROGRESS:
        default:
            break;
    }
}

bool GameEngine::isBlockDeepSleepScene() const {
    return curSceneID == SCENE_BATTLE ||
           curSceneID == SCENE_LOBBY ||
           curSceneID == SCENE_EXPLORE ||
           curSceneID == SCENE_CUP ||
           curSceneID == SCENE_MENU;
}

bool GameEngine::isGameTimePausedScene() const {
    switch (curSceneID) {
        case SCENE_MENU:
        case SCENE_INFO:
        case SCENE_SETTINGS:
        case SCENE_LOBBY:
        case SCENE_EXPLORE:
        case SCENE_CUP:
        case SCENE_BATTLE:
            return true;
        default:
            return false;
    }
}

uint32_t GameEngine::targetFrameTime() const {
    if (systemState != SystemState::ACTIVE) {
        return IDLE_FRAME_MS;
    }
    return ACTIVE_FRAME_MS;
}

void GameEngine::ensureDefaultProgression() {
    progression.foodUnlockMask |= (uint16_t)(1U << (uint8_t)FoodType::DROP);
    uint16_t validFoodMask = (uint16_t)((1U << (uint8_t)FoodType::COUNT) - 1U);
    progression.foodUnlockMask &= validFoodMask;
    if (progression.activeToyBallDurability > TOY_BALL_MAX_DURABILITY) {
        progression.activeToyBallDurability = TOY_BALL_MAX_DURABILITY;
    }
    if (!isMainSceneBgUnlocked(mainSceneBg)) mainSceneBg = BG_MOSS;
    if (!isFoodUnlocked((FoodType)foodStyle)) foodStyle = (uint8_t)FoodType::DROP;
    if (toyStyle == TOY_BALL &&
        (!isToyStyleUnlocked(TOY_BALL) || progression.activeToyBallDurability == 0)) {
        toyStyle = TOY_NONE;
    }
}

void GameEngine::saveProgression() {
    ensureDefaultProgression();
    SaveManager::ins().saveProgression(progression);
}

void GameEngine::forceSave() {
    if (curSceneID == SCENE_TERRARIUM && curScene) {
        static_cast<TerrariumScene*>(curScene)->persistViewState();
    }
    SaveManager::ins().save(bug);
    SaveManager::ins().saveTerrariumViewState(terrariumViewState);
    saveExploreGlobal();
    saveProgression();
}

void GameEngine::resetIdleTimer() {
    idleTimer = 0;
    lastFrameTime = Hal::ins().millis();
    systemState = SystemState::ACTIVE;
}

void GameEngine::setIdleTimeoutIndex(uint8_t idx) {
    if (idx > 4) idx = 4;
    idleTimeoutIndex = idx;
}

void GameEngine::setMainSceneBg(uint8_t id) {
    if (id >= BG_COUNT) id = BG_MOSS;
    if (!isMainSceneBgUnlocked(id)) id = BG_MOSS;
    mainSceneBg = id;
}

void GameEngine::cycleMainSceneBg() {
    uint8_t start = mainSceneBg;
    do {
        mainSceneBg++;
        if (mainSceneBg >= BG_COUNT) mainSceneBg = BG_MOSS;
    } while (mainSceneBg != start && !isMainSceneBgUnlocked(mainSceneBg));
}

bool GameEngine::isMainSceneBgUnlocked(uint8_t id) const {
    switch (id) {
        case BG_MOSS:
        case BG_BEGINNER:
        case BG_CHILD_ROOM:
            return true;
        case BG_ENTOMOLOGIST:
            return isFeatureUnlocked(FEATURE_BG_LAB_UNLOCKED);
        case BG_SCHOOL:
            return isFeatureUnlocked(FEATURE_BG_SCHOOL_UNLOCKED);
        default:
            return false;
    }
}

bool GameEngine::unlockMainSceneBg(uint8_t id) {
    switch (id) {
        case BG_ENTOMOLOGIST:
            return unlockFeature(FEATURE_BG_LAB_UNLOCKED);
        case BG_SCHOOL:
            return unlockFeature(FEATURE_BG_SCHOOL_UNLOCKED);
        default:
            return false;
    }
}

const char* GameEngine::getMainSceneBgName() const {
    switch (mainSceneBg) {
        case BG_BEGINNER: return UiStrings::BG_GIRL;
        case BG_CHILD_ROOM: return UiStrings::BG_BOY;
        case BG_ENTOMOLOGIST: return UiStrings::BG_LAB;
        case BG_SCHOOL: return UiStrings::BG_SCHOOL;
        case BG_MOSS:
        default:
            return UiStrings::BG_ROOM;
    }
}

void GameEngine::setWoodStyle(uint8_t id) {
    if (id != 0xFF && id >= WoodTypeInfo::COUNT) id = 0;
    woodStyle = id;
}

void GameEngine::cycleWoodStyle() {
    woodStyle++;
    if (woodStyle >= WoodTypeInfo::COUNT) woodStyle = 0;
}

const char* GameEngine::getWoodStyleName() const {
    if (woodStyle == 0xFF) return UiStrings::WOOD_NONE;
    return WoodTypeInfo::name((WoodType)woodStyle);
}

void GameEngine::setBowlStyle(uint8_t id) {
    if (id == 0xFF) {
        bowlStyle = 0;
        return;
    }
    if (id >= 3) id = 0;
    if (isBowlStyleUnlocked(id)) {
        bowlStyle = id;
        return;
    }
    uint8_t wins = getBug().getWins();
    // Lv.2 需 2 胜，Lv.3 需 5 胜
    if (id >= 2 && wins < 5) id = 1;
    if (id >= 1 && wins < 2) id = 0;
    bowlStyle = id;
}

void GameEngine::cycleBowlStyle() {
    uint8_t start = bowlStyle;
    do {
        bowlStyle++;
        if (bowlStyle >= 3) bowlStyle = 0;
    } while (bowlStyle != start && !isBowlStyleUnlocked(bowlStyle));
}

bool GameEngine::isBowlStyleUnlocked(uint8_t id) const {
    if (id == 0xFF) return true;
    if (isFeatureUnlocked(FEATURE_BOWL_ALL_UNLOCKED)) return id < 3;
    uint8_t wins = getBug().getWins();
    if (id == 0) return true;
    if (id == 1) return wins >= 2;
    if (id == 2) return wins >= 5;
    return false;
}

const char* GameEngine::getBowlStyleName() const {
    if (bowlStyle == 0xFF) return UiStrings::WOOD_NONE;
    switch (bowlStyle) {
        case 1: return BowlAssets::NAME[1];
        case 2: return BowlAssets::NAME[2];
        case 0:
        default:
            return BowlAssets::NAME[0];
    }
}

void GameEngine::setFoodStyle(uint8_t id) {
    if (id >= 6) id = 0;
    if (!isFoodUnlocked((FoodType)id)) id = (uint8_t)FoodType::DROP;
    foodStyle = id;
}

void GameEngine::cycleFoodStyle() {
    uint8_t start = foodStyle;
    do {
        foodStyle++;
        if (foodStyle >= (uint8_t)FoodType::COUNT) foodStyle = 0;
    } while (foodStyle != start && !isFoodUnlocked((FoodType)foodStyle));
}

bool GameEngine::isFoodUnlocked(FoodType type) const {
    uint8_t idx = (uint8_t)type;
    if (idx >= (uint8_t)FoodType::COUNT) return false;
    return (progression.foodUnlockMask & (uint16_t)(1U << idx)) != 0;
}

bool GameEngine::unlockFood(FoodType type) {
    uint8_t idx = (uint8_t)type;
    if (idx >= (uint8_t)FoodType::COUNT) return false;
    uint16_t bit = (uint16_t)(1U << idx);
    if ((progression.foodUnlockMask & bit) != 0) return false;
    progression.foodUnlockMask |= bit;
    saveProgression();
    return true;
}

const char* GameEngine::getFoodStyleName() const {
    return FoodTypeInfo::name((FoodType)foodStyle);
}

void GameEngine::setToyStyle(uint8_t id) {
    if (id >= TOY_COUNT) id = TOY_NONE;
    if (id != TOY_NONE && !isToyStyleUnlocked(id)) id = TOY_NONE;
    if (id == TOY_BALL && progression.activeToyBallDurability == 0) id = TOY_NONE;
    toyStyle = id;
}

bool GameEngine::isToyStyleUnlocked(uint8_t id) const {
    if (id == TOY_NONE) return true;
    if (id == TOY_BALL) return isFeatureUnlocked(FEATURE_TOY_BALL_UNLOCKED);
    return false;
}

void GameEngine::addToyBall(uint8_t amount) {
    if (amount == 0) return;
    unlockFeature(FEATURE_TOY_BALL_UNLOCKED);
    uint16_t sum = (uint16_t)progression.toyBallCount + amount;
    progression.toyBallCount = sum > 255 ? 255 : (uint8_t)sum;
    saveProgression();
}

bool GameEngine::equipToyBall() {
    if (!isToyStyleUnlocked(TOY_BALL)) return false;
    if (progression.activeToyBallDurability == 0) {
        if (progression.toyBallCount == 0) return false;
        progression.toyBallCount--;
        progression.activeToyBallDurability = TOY_BALL_MAX_DURABILITY;
    }
    toyStyle = TOY_BALL;
    saveProgression();
    return true;
}

bool GameEngine::damageActiveToyBall() {
    if (toyStyle != TOY_BALL || progression.activeToyBallDurability == 0) return false;
    progression.activeToyBallDurability--;
    bool broken = progression.activeToyBallDurability == 0;
    if (broken) {
        toyStyle = TOY_NONE;
        saveSettingsSnapshot();
    }
    saveProgression();
    return broken;
}

void GameEngine::clearActiveToyBall() {
    if (progression.activeToyBallDurability == 0) return;
    progression.activeToyBallDurability = 0;
    if (toyStyle == TOY_BALL) toyStyle = TOY_NONE;
    saveSettingsSnapshot();
    saveProgression();
}

const char* GameEngine::getToyStyleName() const {
    switch (toyStyle) {
        case TOY_BALL: return UiStrings::TOY_BALL;
        case TOY_NONE:
        default:
            return UiStrings::TOY_NONE;
    }
}

bool GameEngine::unlockFeature(ProgressFeature feature) {
    uint32_t bit = (uint32_t)feature;
    if ((progression.featureFlags & bit) != 0) return false;
    progression.featureFlags |= bit;
    saveProgression();
    return true;
}

void GameEngine::unlockAllProgression() {
    progression.featureFlags |= FEATURE_CUP_UNLOCKED |
                                FEATURE_TOY_BALL_UNLOCKED |
                                FEATURE_BG_LAB_UNLOCKED |
                                FEATURE_BG_SCHOOL_UNLOCKED |
                                FEATURE_BOWL_ALL_UNLOCKED;
    progression.foodUnlockMask = (uint16_t)((1U << (uint8_t)FoodType::COUNT) - 1U);
    if (progression.toyBallCount < 9) progression.toyBallCount = 9;
    Bug& bug = getBug();
    for (uint8_t i = 0; i < WoodTypeInfo::COUNT; ++i) {
        bug.addWood(i, 1);
    }
    saveProgression();
    forceSave();
}

uint8_t GameEngine::getItemCount(ItemId id) const {
    switch (ItemCatalog::kind(id)) {
        case ItemKind::FOOD:
        case ItemKind::WOOD:
            return bug.getItemCount(id);
        case ItemKind::TOY:
            return ItemCatalog::index(id) == TOY_BALL ? progression.toyBallCount : 0;
        default:
            return 0;
    }
}

bool GameEngine::addItem(ItemId id, uint8_t amount) {
    if (amount == 0 || !ItemCatalog::isExchangeable(id)) return false;
    switch (ItemCatalog::kind(id)) {
        case ItemKind::FOOD:
            unlockFood((FoodType)ItemCatalog::index(id));
            return bug.addItem(id, amount);
        case ItemKind::WOOD:
            return bug.addItem(id, amount);
        case ItemKind::TOY:
            if (ItemCatalog::index(id) != TOY_BALL) return false;
            addToyBall(amount);
            return true;
        default:
            return false;
    }
}

bool GameEngine::removeItem(ItemId id, uint8_t amount) {
    if (amount == 0 || getItemCount(id) < amount) return false;
    switch (ItemCatalog::kind(id)) {
        case ItemKind::FOOD:
        case ItemKind::WOOD:
            return bug.removeItem(id, amount);
        case ItemKind::TOY:
            if (ItemCatalog::index(id) != TOY_BALL) return false;
            progression.toyBallCount -= amount;
            saveProgression();
            return true;
        default:
            return false;
    }
}

void GameEngine::setExploreLocation(uint8_t id) {
    if (id >= EXPLORE_LOCATION_COUNT) id = EXPLORE_PARK;
    exploreLocation = id;
}

const char* GameEngine::getExploreLocationName() const {
    switch (exploreLocation) {
        case EXPLORE_BACK_HILL: return UiStrings::LOCATION_BACK_HILL;
        case EXPLORE_RIVERSIDE: return UiStrings::LOCATION_RIVERSIDE;
        case EXPLORE_OLD_WOODS: return UiStrings::LOCATION_OLD_WOODS;
        case EXPLORE_PARK:
        default:
            return UiStrings::LOCATION_PARK;
    }
}

const char* GameEngine::getTimeOfDayName() const {
    switch (timeOfDay) {
        case TIME_AFTERNOON: return UiStrings::TIME_AFTERNOON;
        case TIME_EVENING: return UiStrings::TIME_EVENING;
        case TIME_MORNING:
        default:
            return UiStrings::TIME_MORNING;
    }
}

const char* GameEngine::getTimeOfDayShortName() const {
    switch (timeOfDay) {
        case TIME_AFTERNOON: return UiStrings::TIME_SHORT_AFTERNOON;
        case TIME_EVENING: return UiStrings::TIME_SHORT_EVENING;
        case TIME_MORNING:
        default:
            return UiStrings::TIME_SHORT_MORNING;
    }
}

void GameEngine::getExploreClockText(char* buf, size_t bufSize) const {
    if (!buf || bufSize == 0) return;
    static constexpr uint64_t HOUR_MS = 60ULL * 60 * 1000;
    static constexpr uint64_t DAY_MS = 24ULL * HOUR_MS;
    uint64_t msInDay = gameNow % DAY_MS;
    uint8_t hour = (uint8_t)(msInDay / HOUR_MS);
    uint8_t minute = (uint8_t)((msInDay % HOUR_MS) / (60ULL * 1000));
    snprintf(buf, bufSize, "%02u:%02u", hour, minute);
}

uint32_t GameEngine::getCurrentGameDay() const {
    return (uint32_t)(gameNow / GAME_DAY_MS);
}

uint8_t GameEngine::naturalExploreTimeOfDay() const {
    return naturalExploreTimeOfDayFromMs(gameNow);
}

uint8_t GameEngine::naturalExploreTimeOfDayFromMs(uint64_t gameNowMs) {
    static constexpr uint64_t HOUR_MS = 60ULL * 60 * 1000;
    uint64_t hour = (gameNowMs % GAME_DAY_MS) / HOUR_MS;
    // 05:00-11:59 早晨，12:00-18:59 下午，19:00-04:59 夜晚
    if (hour >= 5 && hour < 12) return TIME_MORNING;
    if (hour >= 12 && hour < 19) return TIME_AFTERNOON;
    return TIME_EVENING;
}

bool GameEngine::isExploreTimeAllowedFromMs(uint64_t gameNowMs) {
    static constexpr uint64_t HOUR_MS = 60ULL * 60 * 1000;
    uint64_t hour = (gameNowMs % GAME_DAY_MS) / HOUR_MS;
    return hour >= 5;
}

bool GameEngine::isExploreTimeAllowed() const {
    return isExploreTimeAllowedFromMs(gameNow);
}

void GameEngine::saveExploreGlobal() {
    SaveManager::ins().saveExploreGlobal(exploreDay, timeOfDay, exploreCountToday);
}

void GameEngine::syncExploreClock(bool persist) {
    uint32_t gameDay = exploreCycleDayFromMs(gameNow);
    uint8_t naturalTod = naturalExploreTimeOfDay();
    bool changed = false;

    if (gameDay > exploreDay) {
        // 进入新的探索日：重置次数，时段按自然时间
        exploreDay = gameDay;
        exploreCountToday = 0;
        timeOfDay = naturalTod;
        changed = true;
    } else if (gameDay < exploreDay) {
        // 游戏时间被重置（如新虫、手动重置），修正全局探索时钟
        exploreDay = gameDay;
        exploreCountToday = 0;
        timeOfDay = naturalTod;
        changed = true;
    } else if (timeOfDay < naturalTod) {
        // 同一天内自然推进时段
        timeOfDay = naturalTod;
        changed = true;
    }

    if (timeOfDay > TIME_EVENING) {
        timeOfDay = TIME_MORNING;
        changed = true;
    }
    if (exploreCountToday > EXPLORE_DAILY_LIMIT) {
        exploreCountToday = EXPLORE_DAILY_LIMIT;
        changed = true;
    }

    if (changed && persist) {
        saveExploreGlobal();
    }
}

bool GameEngine::canExplore() const {
    if (bug.getStage() != Stage::ADULT) return false;
    if (bug.isDead()) return false;
    if (bug.getHunger() < 30) return false;
    if (bug.getMot() < 50) return false;
    if (!isExploreTimeAllowed()) return false;
    return isExploreLimitBypassed() || exploreCountToday < EXPLORE_DAILY_LIMIT;
}

bool GameEngine::isExploreLimitBypassed() {
#ifdef POKEBUG_TEST_UNLOCK_EXPLORE
    return true;
#else
    return false;
#endif
}

void GameEngine::recordExploreFinished() {
    if (exploreCountToday < EXPLORE_DAILY_LIMIT) exploreCountToday++;
    gameNow += EXPLORE_FINISH_ADVANCE_MS;
    bug.skipSimulationTime(EXPLORE_FINISH_ADVANCE_MS);
    syncExploreClock(false);
    forceSave();
}

bool GameEngine::canSleep() const {
    if (bug.isDead()) return false;
    static constexpr uint64_t HOUR_MS = 60ULL * 60 * 1000;
    static constexpr uint64_t DAY_MS = 24ULL * HOUR_MS;
    uint64_t hour = (gameNow % DAY_MS) / HOUR_MS;
    // 主动睡眠窗口：20:00 到次日 04:59。
    return hour >= 20 || hour < 5;
}

bool GameEngine::sleepUntilMorning() {
    static constexpr uint64_t HOUR_MS = 60ULL * 60 * 1000;
    static constexpr uint64_t DAY_MS = 24ULL * HOUR_MS;
    static constexpr uint64_t MORNING_MS = 6ULL * HOUR_MS;
    static constexpr uint8_t ACTIVE_SLEEP_MIN_HUNGER = 10;

    if (!canSleep()) return false;

    uint64_t msInDay = gameNow % DAY_MS;
    uint64_t currentHour = msInDay / HOUR_MS;

    uint64_t targetMs;
    if (currentHour >= 20) {
        // 20:00 后：推进到次日 06:00
        targetMs = ((gameNow / DAY_MS) + 1) * DAY_MS + MORNING_MS;
    } else if (currentHour < 5) {
        // 凌晨：推进到当日 06:00
        targetMs = (gameNow / DAY_MS) * DAY_MS + MORNING_MS;
    } else {
        return false;
    }

    if (targetMs <= gameNow) return false;

    uint64_t delta = targetMs - gameNow;
    gameNow = targetMs;

    // 推进期间每 10 分钟更新一次甲虫，避免单次 delta 过大导致饥饿计算溢出。
    // 玩家选择睡到早晨时，甲虫也视为睡眠状态：饥饿下降更慢，低饥饿会自动吃盘中食物。
    static constexpr uint64_t UPDATE_STEP_MS = 10ULL * 60 * 1000;
    uint64_t updated = 0;
    bug.ensureMinHunger(ACTIVE_SLEEP_MIN_HUNGER);
    bug.setSleeping(true);
    while (updated + UPDATE_STEP_MS < delta) {
        updated += UPDATE_STEP_MS;
        updateBugWithStageDefaults(gameNow - delta + updated);
        bug.ensureMinHunger(ACTIVE_SLEEP_MIN_HUNGER);  // 主动睡眠不会把 HUN 压到濒死值
    }
    updateBugWithStageDefaults(gameNow);
    bug.setSleeping(false);
    bug.ensureMinHunger(ACTIVE_SLEEP_MIN_HUNGER);

    syncExploreClock(true);
    forceSave();

    Serial.printf("[Engine] Sleep until morning: delta=%llums, gameNow=%llu\n", delta, gameNow);
    return true;
}

uint32_t GameEngine::getIdleTimeoutMs() const {
    // 0 = never
    static const uint32_t TIMEOUT_MS[5] = { 30000, 60000, 120000, 300000, 0 };
    return TIMEOUT_MS[idleTimeoutIndex];
}

void GameEngine::setPendingNpcBattle(const NpcCombatant& npc, SceneID returnScene, bool legend, bool fromExplore, bool fromCup) {
    npcBattle.active = true;
    npcBattle.returnScene = returnScene;
    npcBattle.siz = npc.siz;
    npcBattle.str = npc.str;
    npcBattle.end = npc.end;
    npcBattle.spd = npc.spd;
    npcBattle.spi = npc.spi;
    npcBattle.mot = npc.mot;
    npcBattle.palette = npc.palette;
    npcBattle.tier = npc.tier;
    npcBattle.legend = legend;
    npcBattle.fromExplore = fromExplore;
    npcBattle.fromCup = fromCup;
    npcResult.valid = false;
    npcResult.won = false;
    npcResult.tier = npc.tier;
    npcResult.legend = legend;
    npcResult.fromExplore = fromExplore;
    npcResult.fromCup = fromCup;
    npcResult.spiBoosted = false;
}
