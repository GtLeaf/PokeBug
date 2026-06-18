#pragma once
#include <cstdint>

// 生命周期阶段
enum class Stage {
    EGG = 0,
    LARVA = 1,
    PUPA = 2,
    ADULT = 3,
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
    // 在食物盘放 1 份树汁；仅当盘中为空且背包有树汁时才成功
    bool placeSapInTray();
    // 戳甲虫（短按 B）
    bool poke(uint64_t now);
    // 剧烈摇晃设备
    bool onShake(uint64_t now);
    // 对战结算
    void onBattleEnd(bool win, uint64_t now);

    // ---------- 阶段与生命 ----------
    Stage getStage() const { return stage; }
    bool isAlive() const { return alive; }
    bool isDead() const { return !alive; }
    bool canAdvanceStage(uint64_t now) const;
    void advanceStage(uint64_t now);

    // ---------- 属性查询 ----------
    float getSiz() const { return siz; }
    float getStr() const { return str; }
    float getEnd() const { return end; }
    float getSpi() const { return spi; }
    uint8_t getMot() const { return mot; }
    uint8_t getHunger() const { return hunger; }

    // ---------- 基因与外观 ----------
    uint8_t getGeneVIG() const { return geneVIG; }
    uint8_t getGeneATK() const { return geneATK; }
    uint8_t getGeneMNT() const { return geneMNT; }
    uint8_t getGeneAPP() const { return geneAPP; }
    uint8_t getPaletteId() const;  // 0-3 由 APP 显性值决定
    const char* getHatchHint() const;

    // ---------- 背包/场景状态 ----------
    uint8_t getSap() const { return sap; }
    uint8_t getRottenWood() const { return rottenWood; }
    bool isWoodPlaced() const { return woodPlaced; }
    bool hasFoodInTray() const { return foodInTray; }
    uint8_t getFoodAmount() const { return foodAmount; }
    bool placeWood();              // 放置腐木到缸内
    void removeWood() { woodPlaced = false; }

    // ---------- 战绩/世代 ----------
    uint8_t getWins() const { return wins; }
    uint8_t getLosses() const { return losses; }
    uint8_t getGeneration() const { return generation; }

    // ---------- 存档 ----------
    void save(uint8_t* buf, uint16_t& len) const;
    bool load(const uint8_t* buf, uint16_t len);

    // 死亡后重置：保留 generation+1，其余清空
    void resetAfterDeath(uint64_t now);

    // 上次更新时间（虚拟时间 ms），供引擎同步
    uint64_t getLastUpdateTime() const { return lastUpdateTime; }

    // 阶段时长（第一阶段使用较短测试值，便于验证；正式上线可改回注释中的值）
    static constexpr uint32_t EGG_DURATION_MS     = 10ULL * 1000;   // 测试 10s；设计值 5 min
    static constexpr uint32_t LARVA_DURATION_MS   = 60ULL * 1000;   // 测试 60s；设计值 30 min
    static constexpr uint32_t PUPA_DURATION_MS    = 20ULL * 1000;   // 测试 20s；设计值 10 min
    static constexpr uint32_t EGG_SHAKE_DELAY_MS  = 30ULL * 1000;   // 每次摇晃卵延长 30s
    static constexpr uint32_t EGG_SHAKE_DELAY_MAX_MS = 2ULL * 60 * 1000; // 累计最多延长 2min
    static constexpr uint32_t HUNGER_DROP_MS      = 90ULL * 1000;   // 饥饿度每 90s -1（节奏更慢，便于观赏）
    static constexpr uint32_t STARVE_DEATH_MS     = 5ULL * 60 * 1000; // 饥饿 0 后持续 5min 死亡
    static constexpr uint32_t ADULT_SAP_PRODUCE_MS = 10ULL * 60 * 1000; // 成虫每 10min 产 1 份树汁
    static constexpr uint8_t  FOOD_MAX_AMOUNT     = 5;              // 一份树汁分成 5 口吃完

    // 戳甲虫机制：愤怒值概率触发 MOT buff，触发后进入冷却；愤怒值会随时间衰减
    static constexpr uint32_t POKE_COOLDOWN_MS       = 30ULL * 1000;       // 触发后冷却 30s
    static constexpr uint32_t MOT_BUFF_DURATION_MS   = 30ULL * 60 * 1000;  // MOT buff 持续 30min
    static constexpr uint8_t  POKE_ANGER_PER_POKE    = 20;                 // 每次戳 +20 愤怒
    static constexpr uint32_t POKE_ANGER_DECAY_MS    = 60ULL * 1000;       // 愤怒每 60s 衰减一次
    static constexpr uint8_t  POKE_ANGER_DECAY_VALUE = 10;                 // 每次衰减 -10
    static constexpr uint8_t  POKE_MOT_BUFF          = 5;                  // 触发后 MOT +5

private:
    uint8_t geneVIG = 0, geneATK = 0, geneMNT = 0, geneAPP = 0;

    float siz = 1.0f, str = 1.0f, end = 1.0f, spi = 1.0f;
    uint8_t mot = 50;
    uint8_t hunger = 100;

    Stage stage = Stage::EGG;
    bool alive = true;

    uint8_t sap = 0;
    uint8_t rottenWood = 0;
    bool woodPlaced = false;
    bool foodInTray = false;
    uint8_t foodAmount = 0;        // 盘中食物剩余口数
    uint64_t lastEatTime = 0;      // 上次进食时间（虚拟时间 ms）

    uint64_t stageStartTime = 0;
    uint64_t lastFeedTime = 0;
    uint64_t lastSapProduceTime = 0;
    uint64_t lastShakeTrainTime = 0;
    uint32_t eggShakeDelayAcc = 0;
    uint64_t restStartTime = 0;
    uint64_t lastPokeTime = 0;

    // 戳甲虫愤怒值与 MOT buff
    uint8_t pokeAnger = 0;              // 当前愤怒值 0-100，决定触发概率
    uint8_t motBuffAmount = 0;          // 当前 MOT buff 增加值
    uint64_t motBuffEndTime = 0;        // MOT buff 结束时间

    uint8_t larvaFeeds = 0;
    uint8_t pupaShakes = 0;
    uint8_t wins = 0;
    uint8_t losses = 0;
    uint8_t generation = 0;

    // 更新用状态
    uint64_t lastUpdateTime = 0;
    uint64_t hungerDropAcc = 0;
    uint64_t deathTimerStart = 0;

    void randomizeGenes();
    void clampAttributes();
    void updateHunger(uint64_t now, uint32_t deltaMs);
    void checkStageTransition(uint64_t now);
    void eatFromTray(uint64_t now);

    static uint8_t dominant(uint8_t gene) { return gene >> 4; }
    static uint8_t recessive(uint8_t gene) { return gene & 0x0F; }
    float sizGrowthMult() const { return 0.8f + dominant(geneVIG) * 0.1f; }
    float strGrowthMult() const { return 0.8f + dominant(geneATK) * 0.1f; }
    float spiGrowthMult() const { return 0.8f + dominant(geneMNT) * 0.1f; }
    uint8_t sizCap() const { return (uint8_t)(6 + (dominant(geneVIG) + recessive(geneVIG)) / 2); }
    uint8_t strCap() const { return (uint8_t)(6 + (dominant(geneATK) + recessive(geneATK)) / 2); }
    uint8_t spiCap() const { return (uint8_t)(6 + (dominant(geneMNT) + recessive(geneMNT)) / 2); }
};
