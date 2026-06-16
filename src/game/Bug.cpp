#include "Bug.h"
#include <Arduino.h>
#include <cmath>

Bug::Bug() {}

void Bug::initNew(uint64_t now) {
    randomizeGenes();

    siz = 1.0f; str = 1.0f; end = 1.0f; spi = 1.0f;
    mot = 50;
    hunger = 100;
    stage = Stage::EGG;
    alive = true;

    sap = 0;
    rottenWood = 0;
    woodPlaced = false;
    foodInTray = false;

    stageStartTime = now;
    lastFeedTime = 0;
    lastSapProduceTime = now;
    lastShakeTrainTime = 0;
    restStartTime = 0;
    lastPokeTime = 0;

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
    // MVP：均匀随机，后期可改为加权分布
    auto rollGene = []() -> uint8_t {
        uint8_t dom = random(16);
        uint8_t rec = random(16);
        return (dom << 4) | rec;
    };
    geneVIG = rollGene();
    geneATK = rollGene();
    geneMNT = rollGene();
    geneAPP = rollGene();
}

void Bug::clampAttributes() {
    if (siz < 1.0f) siz = 1.0f;
    if (siz > sizCap()) siz = sizCap();
    if (str < 1.0f) str = 1.0f;
    if (str > strCap()) str = strCap();
    if (end < 1.0f) end = 1.0f;
    if (end > 10.0f) end = 10.0f;
    if (spi < 1.0f) spi = 1.0f;
    if (spi > spiCap()) spi = spiCap();
    if (mot > 100) mot = 100;
    if (hunger > 100) hunger = 100;
}

void Bug::update(uint64_t now) {
    if (!alive || now <= lastUpdateTime) return;

    uint32_t delta = (uint32_t)(now - lastUpdateTime);
    lastUpdateTime = now;

    // 饥饿度自然下降
    updateHunger(now, delta);
    if (!alive) return;  // 饥饿致死则不再执行后续成长

    // 成虫产树汁
    if (stage == Stage::ADULT) {
        if (now - lastSapProduceTime >= ADULT_SAP_PRODUCE_MS && sap < 6) {
            sap++;
            lastSapProduceTime = now;
        }
    }

    // 食物盘视觉效果自动清除
    if (foodInTray && now - lastFeedTime > 2000) {
        foodInTray = false;
    }

    // 腐木休息 END 成长
    if (stage == Stage::ADULT && woodPlaced && hunger >= 50) {
        if (restStartTime == 0) restStartTime = now;
        if (now - restStartTime >= 2ULL * 60 * 1000) {
            end += 0.3f;
            restStartTime = now;  // 每次休息只加一次，需要离开再回来
        }
    } else {
        restStartTime = 0;
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

    // 饥饿致死
    if (hunger == 0) {
        if (deathTimerStart == 0) deathTimerStart = now;
        else if (now - deathTimerStart >= STARVE_DEATH_MS) {
            alive = false;
        }
    } else {
        deathTimerStart = 0;
    }

    // 饥饿影响 MOT 上限
    if (hunger < 30 && mot > 50) mot = 50;
}

bool Bug::canAdvanceStage(uint64_t now) const {
    uint64_t elapsed = now - stageStartTime;
    switch (stage) {
        case Stage::EGG:
            return elapsed >= EGG_DURATION_MS;
        case Stage::LARVA:
            return elapsed >= LARVA_DURATION_MS;
        case Stage::PUPA:
            return elapsed >= PUPA_DURATION_MS;
        default:
            return false;
    }
}

void Bug::checkStageTransition(uint64_t now) {
    // 允许一次 update 内连续推进多个阶段（catch-up）
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
            sap = 6;  // 幼虫期初始 6 份树汁
            break;
        case Stage::LARVA:
            stageStartTime += LARVA_DURATION_MS;
            stage = Stage::PUPA;
            str += 1.0f;
            siz += 0.5f;
            rottenWood = 1;  // 化蛹获得 1 块腐木
            break;
        case Stage::PUPA:
            stageStartTime += PUPA_DURATION_MS;
            stage = Stage::ADULT;
            spi += 3.0f - (float)pupaShakes;
            break;
        default:
            return;
    }
    clampAttributes();
}

bool Bug::placeSapInTray() {
    if (sap == 0) return false;

    if (stage == Stage::LARVA) {
        sap--;
        str += 0.3f * strGrowthMult();
        siz += 0.15f * sizGrowthMult();
        hunger += 30;
        larvaFeeds++;
        lastFeedTime = lastUpdateTime;
        foodInTray = true;
        clampAttributes();
        return true;
    }

    if (stage == Stage::ADULT) {
        if (lastUpdateTime - lastFeedTime < 5ULL * 60 * 1000) return false;
        sap--;
        end += 0.15f;
        hunger += 30;
        lastFeedTime = lastUpdateTime;
        foodInTray = true;
        clampAttributes();
        return true;
    }

    // 卵/蛹不能喂食
    return false;
}

bool Bug::placeWood() {
    if (rottenWood == 0 || woodPlaced) return false;
    rottenWood--;
    woodPlaced = true;
    return true;
}

bool Bug::poke(uint64_t now) {
    if (now - lastPokeTime < 30ULL * 1000) return false;
    lastPokeTime = now;
    mot += 5;
    if (mot > 100) mot = 100;
    if (hunger < 30 && mot > 50) mot = 50;
    return true;
}

bool Bug::onShake(uint64_t now) {
    switch (stage) {
        case Stage::EGG:
            // 摇晃延长孵化时间：直接推进阶段起始时间
            if (stageStartTime > 30ULL * 1000) stageStartTime -= 30ULL * 1000;
            else stageStartTime = 0;
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

void Bug::onBattleEnd(bool win, uint64_t now) {
    (void)now;
    if (win) {
        wins++;
        spi += 0.5f;
        if (sap + 2 <= 6) sap += 2;
        else sap = 6;
    } else {
        losses++;
        spi += 0.2f;
    }
    mot = 50;
    clampAttributes();
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
    if (total >= 30) return "活力十足！";
    if (total >= 20) return "看起来很健康";
    if (total >= 10) return "有点安静…";
    return "似乎不太好养";
}

void Bug::resetAfterDeath(uint64_t now) {
    uint8_t prevGen = generation;
    initNew(now);
    generation = prevGen + 1;
}

// ---------- 存档格式 ----------
// 按设计文档 save_data_t 打包，约 40 字节
static constexpr uint8_t SAVE_VERSION = 2;

void Bug::save(uint8_t* buf, uint16_t& len) const {
    struct SaveData {
        uint8_t version;
        uint8_t geneVIG, geneATK, geneMNT, geneAPP;
        uint8_t siz, str, end, spi;
        uint8_t mot, hunger;
        uint8_t stage, alive;
        uint8_t sap, rottenWood, woodPlaced;
        uint32_t stageStart;
        uint32_t lastFeed;
        uint32_t lastSap;
        uint32_t lastShake;
        uint32_t restStart;
        uint32_t lastUpdate;
        uint8_t larvaFeeds, pupaShakes;
        uint8_t wins, losses;
        uint8_t generation;
        uint8_t foodInTray;
        uint8_t reserved[2];
    } __attribute__((packed));

    SaveData sd = {};
    sd.version = SAVE_VERSION;
    sd.geneVIG = geneVIG; sd.geneATK = geneATK; sd.geneMNT = geneMNT; sd.geneAPP = geneAPP;
    sd.siz = (uint8_t)roundf(siz);
    sd.str = (uint8_t)roundf(str);
    sd.end = (uint8_t)roundf(end);
    sd.spi = (uint8_t)roundf(spi);
    sd.mot = mot;
    sd.hunger = hunger;
    sd.stage = (uint8_t)stage;
    sd.alive = alive ? 1 : 0;
    sd.sap = sap;
    sd.rottenWood = rottenWood;
    sd.woodPlaced = woodPlaced ? 1 : 0;
    sd.stageStart = (uint32_t)(stageStartTime / 1000ULL);
    sd.lastFeed = (uint32_t)(lastFeedTime / 1000ULL);
    sd.lastSap = (uint32_t)(lastSapProduceTime / 1000ULL);
    sd.lastShake = (uint32_t)(lastShakeTrainTime / 1000ULL);
    sd.restStart = (uint32_t)(restStartTime / 1000ULL);
    sd.lastUpdate = (uint32_t)(lastUpdateTime / 1000ULL);
    sd.larvaFeeds = larvaFeeds;
    sd.pupaShakes = pupaShakes;
    sd.wins = wins;
    sd.losses = losses;
    sd.generation = generation;
    sd.foodInTray = foodInTray ? 1 : 0;

    memcpy(buf, &sd, sizeof(sd));
    len = sizeof(sd);
}

bool Bug::load(const uint8_t* buf, uint16_t len) {
    struct SaveData {
        uint8_t version;
        uint8_t geneVIG, geneATK, geneMNT, geneAPP;
        uint8_t siz, str, end, spi;
        uint8_t mot, hunger;
        uint8_t stage, alive;
        uint8_t sap, rottenWood, woodPlaced;
        uint32_t stageStart;
        uint32_t lastFeed;
        uint32_t lastSap;
        uint32_t lastShake;
        uint32_t restStart;
        uint32_t lastUpdate;
        uint8_t larvaFeeds, pupaShakes;
        uint8_t wins, losses;
        uint8_t generation;
        uint8_t foodInTray;
        uint8_t reserved[2];
    } __attribute__((packed));

    if (len != sizeof(SaveData)) return false;
    const SaveData& sd = *reinterpret_cast<const SaveData*>(buf);
    if (sd.version != SAVE_VERSION) return false;

    geneVIG = sd.geneVIG; geneATK = sd.geneATK; geneMNT = sd.geneMNT; geneAPP = sd.geneAPP;
    this->siz = sd.siz; this->str = sd.str; this->end = sd.end; this->spi = sd.spi;
    mot = sd.mot;
    hunger = sd.hunger;
    stage = (Stage)sd.stage;
    alive = sd.alive != 0;
    sap = sd.sap;
    rottenWood = sd.rottenWood;
    woodPlaced = sd.woodPlaced != 0;
    stageStartTime = (uint64_t)sd.stageStart * 1000ULL;
    lastFeedTime = (uint64_t)sd.lastFeed * 1000ULL;
    lastSapProduceTime = (uint64_t)sd.lastSap * 1000ULL;
    lastShakeTrainTime = (uint64_t)sd.lastShake * 1000ULL;
    restStartTime = (uint64_t)sd.restStart * 1000ULL;
    lastUpdateTime = (uint64_t)sd.lastUpdate * 1000ULL;
    larvaFeeds = sd.larvaFeeds;
    pupaShakes = sd.pupaShakes;
    wins = sd.wins;
    losses = sd.losses;
    generation = sd.generation;
    foodInTray = sd.foodInTray != 0;

    clampAttributes();
    return true;
}
