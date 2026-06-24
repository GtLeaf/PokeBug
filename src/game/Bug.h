#pragma once
#include <Arduino.h>
#include <cstdint>
#include "FoodType.h"
#include "ItemCatalog.h"

// 生命周期阶段
enum class Stage {
    EGG = 0,
    LARVA = 1,
    PUPA = 2,
    JUVENILE = 3,
    ADULT = 4,
};

// 卵期气质
enum class Temperament : uint8_t {
    SWIFT = 0,      // 迅捷
    RESILIENT,      // 韧甲
    GIANT,          // 巨体
    BRUTE,          // 蛮力
    BALANCED,       // 均衡
    SPIRIT,         // 灵心
};

// 卵期操作类型
enum class EggAction : uint8_t {
    NONE = 0,
    POKE,
    WATER,
    SHAKE,
};

// 独角仙实体：属性、生命周期、饥饿、基因、行为
class Bug {
public:
    Bug();

    // 初始化一只新的野生独角仙（ generation = 0 ）
    void initNew(uint64_t now);

    // 主更新，传入虚拟时间（ms）
    void update(uint64_t now);

    // ---------- 玩家交互 ----------
    // 在食物盘放 1 份指定食物；仅当盘中为空且背包有该食物时才成功
    bool placeFoodInTray(FoodType type);
    // 喂食（甲虫自主进食时调用）
    void feed(FoodType type, uint64_t now);
    // 戳甲虫（短按 B）
    bool poke(uint64_t now);
    // 剧烈摇晃设备
    bool onShake(uint64_t now);
    // 喷水（卵期）
    void onWater(uint64_t now);
    // 记录卵期戳
    void onEggPoke(uint64_t now);
    // 记录卵期喷水
    void onEggWater(uint64_t now);
    // 记录卵期摇晃/倾斜
    void onEggShake(uint64_t now, bool violent);
    void onEggTilt(uint64_t now, uint32_t deltaMs, bool left);
    // 对战结算
    float onBattleEnd(bool win, uint64_t now, float spiReward = -1.0f);

    // ---------- 阶段与生命 ----------
    Stage getStage() const { return stage; }
    bool isAlive() const { return alive; }
    bool isDead() const { return !alive; }
    bool canAdvanceStage(uint64_t now) const;
    void advanceStage(uint64_t now);
    void debugSetStage(Stage nextStage, uint64_t now);
    void debugSetTemperament(Temperament nextTemperament);
    float debugGetAttr(uint8_t index) const;
    uint8_t debugGetAttrCap(uint8_t index) const;
    void debugSetAttr(uint8_t index, float value);

    // ---------- 属性查询 ----------
    float getSiz() const { return siz; }
    float getStr() const { return str; }
    float getEnd() const { return end; }
    float getSpd() const { return spd; }
    float getSpi() const { return spi; }
    uint8_t getSizCap() const { return sizCap(); }
    uint8_t getStrCap() const { return strCap(); }
    uint8_t getEndCap() const { return endCap(); }
    uint8_t getSpdCap() const { return spdCap(); }
    uint8_t getSpiCap() const { return spiCap(); }
    uint8_t getMot() const { return mot; }
    void setMot(uint8_t value) { mot = value > 100 ? 100 : value; }
    uint8_t getHunger() const { return hunger; }
    void modHunger(int8_t delta);
    void ensureMinHunger(uint8_t minHunger = 1);
    bool eatSubstrate(uint64_t now);
    void skipSimulationTime(uint64_t deltaMs);
    void addTrainingBonus(float sizDelta, float strDelta, float endDelta,
                          float spdDelta, float spiDelta);

    // ---------- 基因与外观 ----------
    uint8_t getGeneVIG() const { return geneVIG; }
    uint8_t getGeneATK() const { return geneATK; }
    uint8_t getGeneMNT() const { return geneMNT; }
    uint8_t getGeneEND() const { return geneEND; }
    uint8_t getGeneAPP() const { return geneAPP; }
    uint8_t getPaletteId() const;  // 0-3 由 APP 显性值决定
    const char* getHatchHint() const;
    Temperament getTemperament() const { return temperament; }
    const char* getTemperamentName() const;
    float getAdultDepth() const;
    float getAdultScale() const;

    // ---------- 背包/场景状态 ----------
    uint8_t getFoodCount(FoodType type) const { return foodCounts[(uint8_t)type]; }
    void addFood(FoodType type, uint8_t amount);
    void removeFood(FoodType type, uint8_t amount);
    uint8_t getTotalFoodCount() const;
    uint8_t getItemCount(ItemId id) const;
    bool addItem(ItemId id, uint8_t amount);
    bool removeItem(ItemId id, uint8_t amount);
    bool addItem(const ItemStack& item) { return addItem(item.id, item.amount); }
    bool removeItem(const ItemStack& item) { return removeItem(item.id, item.amount); }
    void setSleeping(bool sleepingNow) { sleeping = sleepingNow; }
    bool isSleeping() const { return sleeping; }
    bool isWoodPlaced() const { return woodPlaced; }
    bool hasFoodInTray() const { return foodInTray; }
    FoodType getFoodInTrayType() const { return trayFoodType; }
    uint8_t getFoodAmount() const { return foodAmount; }
    uint64_t getLastEatTime() const { return lastEatTime; }
    bool placeWood();              // 放置腐木到缸内
    void removeWood() { woodPlaced = false; }
    bool isWoodUnlocked(uint8_t style) const;
    uint8_t getWoodCount(uint8_t style) const;
    uint8_t getTotalWoodCount() const;
    uint8_t getRottenWood() const { return getTotalWoodCount(); }
    void addWood(uint8_t style, uint8_t amount = 1);
    void removeWoodItem(uint8_t style, uint8_t amount = 1);
    void addRottenWood(uint8_t amount = 1);

    // 环境加成
    void setFoodTray(uint8_t level, FoodType type);
    void setWood(uint8_t style);
    float getEnvMultiplier(int attrIndex) const; // 0=SIZ,1=STR,2=END,3=SPD,4=SPI

    // 自主进食：幼虫/蛹期在 Bug::update 中调用；成虫由 TerrariumScene 在 EAT 状态时调用
    bool eatFromTray(uint64_t now, bool forceBite = false);
    // 成虫真正趴在腐木上休息时调用；连续休息才获得 END 成长
    void recordWoodRest(uint64_t now);

    // ---------- 战绩/世代 ----------
    uint8_t getWins() const { return wins; }
    uint8_t getLosses() const { return losses; }
    uint8_t getGeneration() const { return generation; }

    // ---------- 存档 ----------
    void save(uint8_t* buf, uint16_t& len) const;
    bool load(const uint8_t* buf, uint16_t len);

    // 死亡后重置：保留 generation+1，其余清空
    void resetAfterDeath(uint64_t now);

    // 放生：基于当前基因小幅变异后产生新卵，generation+1
    void release(uint64_t now);

    // ---------- 探索 & 杯赛记录 ----------
    uint8_t getReleaseCountTotal() const { return releaseCountTotal; }
    uint16_t getCupParticipated() const { return cupParticipated; }
    uint8_t getCupBest() const { return cupBest; }
    uint8_t getCupWins() const { return cupWins; }
    uint8_t getCupLegendKills() const { return cupLegendKills; }
    uint16_t getAchievementFlags() const { return achievementFlags; }
    uint8_t getCupStreak() const { return cupStreak; }
    float getStageProgress(uint64_t now) const;

    void addReleaseCount();
    void recordCupParticipation();
    void recordCupResult(uint8_t rank); // rank: 1=冠军,2=亚军,4=四强,8=八强
    void recordCupWin();
    void recordCupLegendKill();
    void setAchievementFlag(uint16_t flag);
    bool hasAchievementFlag(uint16_t flag) const;
    void resetCupStreak();
    void incrementCupStreak();

    // 上次更新时间（虚拟时间 ms），供引擎同步
    uint64_t getLastUpdateTime() const { return lastUpdateTime; }

    // 阶段时长（按设计文档 v1.0）
    static constexpr uint32_t EGG_DURATION_MS      = 10ULL * 60 * 1000;  // 10 min
    static constexpr uint32_t LARVA_DURATION_MS    = 60ULL * 60 * 1000;  // 60 min
    static constexpr uint32_t PUPA_DURATION_MS     = 20ULL * 60 * 1000;  // 20 min
    static constexpr uint32_t JUVENILE_DURATION_MS = 60ULL * 60 * 1000;  // 60 min
    static constexpr uint32_t EGG_SHAKE_DELAY_MS  = 30ULL * 1000;   // 每次摇晃卵延长 30s
    static constexpr uint32_t EGG_SHAKE_DELAY_MAX_MS = 2ULL * 60 * 1000; // 累计最多延长 2min
    static constexpr uint32_t HUNGER_DROP_MS      = 90ULL * 1000;   // 饥饿度每 90s -1（节奏更慢，便于观赏）
    static constexpr uint32_t LARVA_HUNGER_DROP_MS = 45ULL * 1000;  // 幼虫代谢更快，每 45s -1
    static constexpr uint32_t LARVA_SUBSTRATE_EAT_MS = 60ULL * 1000; // 啃底材每口间隔
    static constexpr uint8_t  LARVA_SUBSTRATE_EAT_HUNGER = 90;      // 低于该值开始啃底材
    static constexpr uint8_t  LARVA_SUBSTRATE_HUNGER_GAIN = 6;      // 每口底材恢复 HUN
    static constexpr uint32_t STARVE_DEATH_MS     = 5ULL * 60 * 1000; // 饥饿 0 后持续 5min 死亡
    static constexpr uint32_t ADULT_SAP_PRODUCE_MS = 10ULL * 60 * 1000; // 成虫每 10min 产 1 份树汁
    static constexpr uint8_t  FOOD_MAX_AMOUNT     = 5;              // 一份食物分成 5 口吃完

    // 戳甲虫机制：愤怒值概率触发 MOT buff，触发后进入冷却；愤怒值会随时间衰减
    static constexpr uint32_t POKE_COOLDOWN_MS       = 30ULL * 1000;       // 触发后冷却 30s
    static constexpr uint32_t MOT_BUFF_DURATION_MS   = 30ULL * 60 * 1000;  // MOT buff 持续 30min
    static constexpr uint8_t  POKE_ANGER_PER_POKE    = 20;                 // 每次戳 +20 愤怒
    static constexpr uint32_t POKE_ANGER_DECAY_MS    = 60ULL * 1000;       // 愤怒每 60s 衰减一次
    static constexpr uint8_t  POKE_ANGER_DECAY_VALUE = 10;                 // 每次衰减 -10
    static constexpr uint8_t  POKE_MOT_BUFF          = 5;                  // 触发后 MOT +5

    // 蛹期安静成长：整个蛹期总计维持原有 SPI +1
    static constexpr uint32_t PUPA_SPI_GROWTH_MS     = PUPA_DURATION_MS;

private:
    uint8_t geneVIG = 0, geneATK = 0, geneMNT = 0, geneEND = 0, geneAPP = 0;

    float siz = 1.0f, str = 1.0f, end = 1.0f, spd = 1.0f, spi = 1.0f;
    uint8_t mot = 50;
    uint8_t hunger = 100;

    Stage stage = Stage::EGG;
    bool alive = true;

    static constexpr uint8_t MAX_FOOD_COUNT = 255;
    uint8_t foodCounts[(uint8_t)FoodType::COUNT] = {0}; // 背包中各食物数量
    uint8_t woodUnlocked[WoodTypeInfo::COUNT] = {0};    // 各腐木是否已解锁
    bool woodPlaced = false;
    bool sleeping = false;
    bool foodInTray = false;
    FoodType trayFoodType = FoodType::DROP;
    uint8_t foodAmount = 0;        // 盘中食物剩余口数
    uint64_t lastEatTime = 0;      // 上次进食时间（虚拟时间 ms）

    uint64_t stageStartTime = 0;
    uint64_t lastFeedTime = 0;
    uint64_t lastSapProduceTime = 0;
    uint64_t lastShakeTrainTime = 0;
    uint32_t eggShakeDelayAcc = 0;
    uint64_t restStartTime = 0;
    uint64_t woodRestAcc = 0;
    uint64_t lastPokeTime = 0;

    // 戳甲虫愤怒值与 MOT buff
    uint8_t pokeAnger = 0;              // 当前愤怒值 0-100，决定触发概率
    uint8_t motBuffAmount = 0;          // 当前 MOT buff 增加值
    uint64_t motBuffEndTime = 0;        // MOT buff 结束时间

    // 卵期记录
    Temperament temperament = Temperament::SPIRIT;
    uint64_t eggStartTime = 0;
    uint8_t eggShakeCount = 0;
    uint8_t eggViolentShakeCount = 0;
    uint8_t eggPokeCount = 0;
    uint8_t eggWaterCount = 0;
    uint32_t eggLeftTiltMs = 0;
    uint32_t eggRightTiltMs = 0;
    uint8_t eggLastAction = 0; // 0=无, 1=戳, 2=喷水, 3=摇晃

    // 环境加成
    uint8_t foodTrayLevel = 1;
    FoodType foodTrayType = FoodType::DROP;
    uint8_t woodStyle = 0;

    // 幼虫期 Citrus 累计次数（影响蛹期 SPI 上限）
    uint8_t larvaCitrusCount = 0;
    // 幼虫期是否有 Berry（影响蛹期安静效率）
    bool larvaBerryFed = false;
    // Citrus 累计 3 次奖励：每 1 = +0.1 SPI 上限
    uint8_t spiCapBonusTenths = 0;

    uint8_t larvaFeeds = 0;
    uint8_t pupaShakes = 0;
    uint8_t wins = 0;
    uint8_t losses = 0;
    uint8_t generation = 0;

    // 探索 & 杯赛记录
    uint8_t releaseCountTotal = 0;
    uint16_t cupParticipated = 0;
    uint8_t cupBest = 0;          // 1=冠军,2=亚军,4=四强,8=八强
    uint8_t cupWins = 0;
    uint8_t cupLegendKills = 0;
    uint16_t achievementFlags = 0;
    uint8_t cupStreak = 0;

    // 更新用状态
    uint64_t lastUpdateTime = 0;
    uint64_t hungerDropAcc = 0;
    uint64_t deathTimerStart = 0;

    void randomizeGenes();
    void clampAttributes();
    void updateHunger(uint64_t now, uint32_t deltaMs);
    void checkStageTransition(uint64_t now);
    Temperament determineTemperament(uint64_t now);
    void updatePupaSpi(uint64_t now, uint32_t deltaMs);

    float spdGrowthMult() const { return 0.8f + dominant(geneAPP) * 0.1f; }
    uint8_t spdCap() const { return (uint8_t)(6 + (dominant(geneAPP) + recessive(geneAPP)) / 2); }
    static uint8_t dominant(uint8_t gene) { return gene >> 4; }
    static uint8_t recessive(uint8_t gene) { return gene & 0x0F; }
    float sizGrowthMult() const { return 0.8f + dominant(geneVIG) * 0.1f; }
    float strGrowthMult() const { return 0.8f + dominant(geneATK) * 0.1f; }
    float endGrowthMult() const { return 0.8f + dominant(geneEND) * 0.1f; }
    float spiGrowthMult() const { return 0.8f + dominant(geneMNT) * 0.1f; }
    uint8_t sizCap() const { return (uint8_t)(6 + (dominant(geneVIG) + recessive(geneVIG)) / 2); }
    uint8_t strCap() const { return (uint8_t)(6 + (dominant(geneATK) + recessive(geneATK)) / 2); }
    uint8_t endCap() const { return (uint8_t)(6 + (dominant(geneEND) + recessive(geneEND)) / 2); }
    uint8_t spiCap() const {
        return (uint8_t)(6 + (dominant(geneMNT) + recessive(geneMNT)) / 2 + spiCapBonusTenths / 10.0f);
    }
};
