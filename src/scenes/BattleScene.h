#pragma once
#include "../core/Scene.h"
#include "../hardware/BattleLink.h"
#include "../game/BattleCalc.h"

// 对战场景（ATB 节奏条驱动）
class BattleScene : public Scene {
public:
    BattleScene() = default;

    void onEnter() override;
    void onExit() override;
    SceneID update() override;
    void render() override;
    bool onButton(const ButtonEvent& ev) override;

private:
    enum class State {
        CONNECTING,
        SYNCING,
        ROUND_START,
        GAUGE_FILLING,
        ATTACK_ONE,
        ATTACK_TWO,
        ROUND_END,
        RESULT,
        DONE,
    };
    State state = State::CONNECTING;

    // 我方/敌方战斗数值
    struct Combatant : public BattleCalc::BattleStats {
        int hp = 0;
        int maxHp = 0;
        uint8_t palette = 0;
    };
    Combatant me;
    Combatant enemy;

    int roundNum = 1;
    bool roundBoosted = false;

    // ATB 节奏条（0.0 ~ 1.0）
    float myGauge = 0.0f;       // 显示用 gauge，与真实触发进度保持一致
    float enemyGauge = 0.0f;
    float myRealGauge = 0.0f;   // 真实触发用 gauge（绝对速度）
    float enemyRealGauge = 0.0f;

    // 本回合计算结果
    int myDmg = 0;
    bool myCrit = false;
    bool myAttackDodged = false;
    int enemyDmg = 0;
    bool enemyCrit = false;
    bool enemyAttackDodged = false;
    bool flashThisFrame = false;
    bool firstAttackByMe = true;
    bool secondAttackPlanned = false;
    int roundEndMyHp = 0;
    int roundEndEnemyHp = 0;
    uint8_t roundEndMyMot = 0;
    uint8_t roundEndEnemyMot = 0;

    // 对战结果
    bool localWin = false;
    bool resultApplied = false;
    bool noOpponent = false;
    bool localNpcBattle = false;
    bool legendNpc = false;
    SceneID returnScene = SCENE_TERRARIUM;

    uint32_t stateStartMs = 0;
    uint32_t lastGaugeUpdateMs = 0;
    uint32_t gaugeReadySinceMs = 0;

    // 受击晃动时间戳
    uint32_t meShakeEndMs = 0;
    uint32_t enemyShakeEndMs = 0;

    // 同步/发送状态
    bool syncSent = false;
    bool syncAcked = false;
    bool foeSyncReceived = false;
    bool roundSent = false;
    bool resultSent = false;
    bool clientReadyReceived = false;

    // 主机 authoritative 发送缓冲（处理 sendBusy 导致的延迟/重试）
    bool roundComputed = false;
    battle_round_t hostRound;
    bool hasPendingClientReady = false;
    battle_ready_t pendingClientReady;

    // 日志看门狗：每 2 秒打印一次当前状态，便于定位卡死
    State lastLoggedState = State::CONNECTING;
    uint32_t lastStateLogMs = 0;
    void maybeLogStateStall();

    void initFromBug();
    bool buildSync();
    void startRound(bool resetGauge = true);
    battle_round_t computeAuthoritativeRound();
    void beginAuthoritativeRound(const battle_round_t& round);
    void applyCurrentAttack();
    bool isCurrentAttackByMe() const;
    bool isCurrentAttackCritical() const;
    bool attackExistsFor(bool byMe) const;
    void computeLocalWin();
    void computeAndSendResult();
    void applyBattleResult();

    // ATB 节奏条
    void updateGauge(uint32_t nowMs);
    bool tryComputeRound();  // 本地/NPC 满足条件时计算攻击计划
    void enterAttackOne();
    void resetGaugeAfterAction();

    void drawConnecting();
    void drawBattleField();
    void drawCombatantSprite(const Combatant& combatant, int centerX, int groundY,
                             bool faceRight, int8_t shakeX, int8_t shakeY,
                             bool attacking, bool critical);
    void drawTempoBar();
    int tempoScore(const Combatant& combatant) const;
    float tempoProgress(bool forMe) const;
    void drawResult();

    static constexpr uint32_t GAUGE_FILL_MS = 2000;      // gauge 填充参考时间（高速可低于 3 秒）
    static constexpr uint32_t GAUGE_FASTEST_MAX_MS = 2000; // 双方都慢时，最快方最多 3 秒充满
    static constexpr uint32_t ATTACK_MS = 700;
    static constexpr uint32_t ROUND_END_MS = 500;
    static constexpr uint32_t SHAKE_MS = 300;
    static constexpr int8_t  SHAKE_AMP = 3;
    static constexpr uint32_t SYNC_TIMEOUT_MS = 5000;
    static constexpr uint32_t ROUND_TIMEOUT_MS = 5000;
    static constexpr uint32_t RESULT_TIMEOUT_MS = 5000;
    static constexpr uint8_t MAX_ROUNDS = 30;
    static constexpr float GAUGE_BASE_SCORE = 80.0f;     // 参考 tempoScore，对应 GAUGE_FILL_MS 充满
};
