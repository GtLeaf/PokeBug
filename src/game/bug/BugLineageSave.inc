    auto clampNibble = [](int v) -> int {
        if (v < 0) return 0;
        if (v > 15) return 15;
        return v;
    };

    auto achievedStage = [](float value, uint8_t cap) -> uint8_t {
        if (cap == 0) return 0;
        float ratio = value / (float)cap;
        if (ratio >= 0.95f) return 4;
        if (ratio >= 0.85f) return 3;
        if (ratio >= 0.70f) return 2;
        if (ratio >= 0.50f) return 1;
        return 0;
    };

    auto inheritByAchievement = [&](uint8_t g, float value, uint8_t cap) -> uint8_t {
        static const uint8_t IMPROVE_CHANCE[5] = {10, 35, 50, 65, 80};
        static const uint8_t REGRESS_CHANCE[5] = {10, 8, 6, 5, 4};

        uint8_t stageScore = achievedStage(value, cap);
        int d = g >> 4;
        int r = g & 0x0F;
        uint8_t roll = (uint8_t)random(100);

        if (roll < IMPROVE_CHANCE[stageScore]) {
            uint8_t steps = 1;
            if (stageScore >= 3 && (uint8_t)random(100) < 35) steps++;
            if (stageScore >= 4 && (uint8_t)random(100) < 20) steps++;

            while (steps-- > 0) {
                if (d <= r || (uint8_t)random(100) < 65) d = clampNibble(d + 1);
                else r = clampNibble(r + 1);
            }
        } else if (roll < IMPROVE_CHANCE[stageScore] + REGRESS_CHANCE[stageScore]) {
            if (d > r || (uint8_t)random(100) < 50) d = clampNibble(d - 1);
            else r = clampNibble(r - 1);
        }

        return (uint8_t)((d << 4) | r);
    };

    // 放生时只取“已达属性阶段分”作为遗传压力，而不是直接继承当前属性值。
    uint8_t nextGeneVIG = inheritByAchievement(geneVIG, siz, sizCap());
    uint8_t nextGeneATK = inheritByAchievement(geneATK, str, strCap());
    uint8_t nextGeneMNT = inheritByAchievement(geneMNT, spi, spiCap());
    uint8_t nextGeneEND = inheritByAchievement(geneEND, end, endCap());
    uint8_t nextGeneAPP = inheritByAchievement(geneAPP, spd, spdCap());
    uint8_t prevGen = generation;

    initNew(now);
    geneVIG = nextGeneVIG;
    geneATK = nextGeneATK;
    geneMNT = nextGeneMNT;
    geneEND = nextGeneEND;
    geneAPP = nextGeneAPP;
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
// v9：腐木从单一 rottenWood 升级为按腐木种类解锁，支持交换/活动限定腐木
static constexpr uint8_t SAVE_VERSION = 9;

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
        uint8_t woodUnlocked[WoodTypeInfo::COUNT];
        uint8_t reserved[1];
    } __attribute__((packed));
    static_assert(sizeof(SaveData) <= 192, "Bug save data exceeds SaveManager buffer");

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
    sd.rottenWood = getTotalWoodCount(); // legacy total for diagnostics / downgrade reading
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
    for (int i = 0; i < WoodTypeInfo::COUNT; i++) sd.woodUnlocked[i] = woodUnlocked[i] ? 1 : 0;

    memcpy(buf, &sd, sizeof(sd));
    len = sizeof(sd);
}


bool Bug::load(const uint8_t* buf, uint16_t len) {
    struct SaveDataV9 {
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
        uint8_t woodUnlocked[WoodTypeInfo::COUNT];
        uint8_t reserved[1];
    } __attribute__((packed));
    static_assert(sizeof(SaveDataV9) <= 192, "Bug save data exceeds SaveManager buffer");

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

    auto applyCommon = [this](const SaveDataV9& sd) {
        geneVIG = sd.geneVIG; geneATK = sd.geneATK; geneMNT = sd.geneMNT; geneEND = sd.geneEND; geneAPP = sd.geneAPP;
        siz = sd.siz; str = sd.str; end = sd.end; spd = sd.spd; spi = sd.spi;
        mot = sd.mot; hunger = sd.hunger;
        stage = (Stage)sd.stage;
        alive = sd.alive != 0;
        for (int i = 0; i < 6; i++) foodCounts[i] = sd.foodCounts[i];
        bool hasUnlockedWood = false;
        for (int i = 0; i < WoodTypeInfo::COUNT; i++) {
            woodUnlocked[i] = sd.woodUnlocked[i] > 0 ? 1 : 0;
            if (woodUnlocked[i]) hasUnlockedWood = true;
        }
        if (!hasUnlockedWood && sd.rottenWood > 0) {
            woodUnlocked[(uint8_t)WoodType::TWIG] = 1;
        }
        woodPlaced = sd.woodPlaced != 0;
        sleeping = false;
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

    if (ver == SAVE_VERSION && len >= sizeof(SaveDataV9)) {
        applyCommon(*reinterpret_cast<const SaveDataV9*>(buf));
        return true;
    }

    if (ver == 8 && len >= sizeof(SaveDataV9) - WoodTypeInfo::COUNT) {
        SaveDataV9 tmp = {};
        uint16_t copyLen = len < sizeof(SaveDataV9) ? len : sizeof(SaveDataV9);
        memcpy(&tmp, buf, copyLen);
        tmp.version = SAVE_VERSION;
        applyCommon(tmp);
        return true;
    }

    if (ver == 7 && len >= sizeof(SaveDataV7)) {
        const SaveDataV7& sd = *reinterpret_cast<const SaveDataV7*>(buf);
        SaveDataV9 tmp = {};
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
        SaveDataV9 tmp = {};
        memcpy(&tmp, &sd, sizeof(SaveDataV6));
        tmp.version = SAVE_VERSION;
        tmp.geneEND = 0x55;
        applyCommon(tmp);
        return true;
    }

    if (ver == 5 && len >= sizeof(SaveDataV5)) {
        const SaveDataV5& sd = *reinterpret_cast<const SaveDataV5*>(buf);
        SaveDataV9 tmp = {};
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
