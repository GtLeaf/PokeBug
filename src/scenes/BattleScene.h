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
        TIMEOUT,
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
    struct RhythmWindow {
        uint8_t opportunity = 0;  // 第几次我方攻击机会出现，1-based
        uint8_t startPx = 0;
        uint8_t widthPx = 0;
    };
    enum class RhythmFeedback : uint8_t {
        NONE,
        MISS,
        GREAT,
        PERFECT,
    };
    Combatant me;
    Combatant enemy;

    int roundNum = 1;

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

    // 玩家侧节奏判定窗口：本地生成，本地显示，本地结算到 me.mot 后参与同步。
    RhythmWindow rhythmWindows[6];
    uint8_t rhythmWindowCount = 0;
    uint8_t rhythmWindowIndex = 0;
    uint8_t myAttackOpportunity = 1;
    bool rhythmPressedThisOpportunity = false;
    bool rhythmRawAPrev = false;
    RhythmFeedback rhythmFeedback = RhythmFeedback::NONE;
    uint32_t rhythmFeedbackUntilMs = 0;

    // A 键长按返回备用检测（ButtonDispatcher 长按偶尔不可靠时兜底）
    uint32_t btnAHoldStartMs = 0;
    bool btnAHoldReturned = false;

    // 受击晃动时间戳
    uint32_t meShakeEndMs = 0;
    uint32_t enemyShakeEndMs = 0;

    // 同步/发送状态
    bool syncSent = false;
    bool syncAcked = false;
    bool foeSyncReceived = false;
    bool roundSent = false;
    bool resultSent = false;
    bool motUpdatePending = false;
    bool motUpdateInFlight = false;
    uint8_t motUpdateRound = 0;
    uint8_t motUpdateValue = 0;

    // 主机 authoritative 发送缓冲（处理 sendBusy 导致的延迟/重试）
    bool roundComputed = false;
    battle_round_t hostRound;

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
    void resetGaugeAfterAction(bool byMe);
    void queueMotUpdate();
    void processMotUpdate();
    void generateRhythmWindows();
    void refreshRhythmWindow();
    const RhythmWindow* currentRhythmWindow() const;
    bool isRhythmWindowActive() const;
    void handleRhythmButtonPress(uint32_t nowMs);
    void finishMyAttackOpportunity();
    void showRhythmFeedback(RhythmFeedback feedback, uint32_t nowMs);

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
    static constexpr uint32_t TIMEOUT_NOTICE_MS = 2000;
    static constexpr uint32_t SHAKE_MS = 300;
    static constexpr int8_t  SHAKE_AMP = 3;
    static constexpr uint32_t SYNC_TIMEOUT_MS = 5000;
    static constexpr uint32_t ROUND_TIMEOUT_MS = 5000;
    static constexpr uint32_t RESULT_TIMEOUT_MS = 5000;
    static constexpr uint8_t MAX_ROUNDS = 40;
    static constexpr float GAUGE_BASE_SCORE = 80.0f;     // 参考 tempoScore，对应 GAUGE_FILL_MS 充满

    static constexpr int TEMPO_LABEL_X = 8;
    static constexpr int TEMPO_BAR_X = 50;
    static constexpr int TEMPO_BAR_Y = 124;
    static constexpr int TEMPO_BAR_W = 152;
    static constexpr int TEMPO_BAR_H = 5;
    static constexpr int TEMPO_ICON_Y = TEMPO_BAR_Y + 2;

    static constexpr uint8_t RHYTHM_INTERVAL_MIN = 6;
    static constexpr uint8_t RHYTHM_INTERVAL_MAX = 8;
    static constexpr uint8_t RHYTHM_ZONE_MIN_PCT = 30;
    static constexpr uint8_t RHYTHM_ZONE_MAX_PCT = 80;
    static constexpr uint8_t RHYTHM_ZONE_W_MIN = 23;
    static constexpr uint8_t RHYTHM_ZONE_W_MAX = 30;
    static constexpr uint8_t RHYTHM_BOOST_MISS = 8;
    static constexpr uint8_t RHYTHM_BOOST_GREAT = 15;
    static constexpr uint8_t RHYTHM_BOOST_PERFECT = 22;
    static constexpr uint32_t RHYTHM_FEEDBACK_MS = 500;
};
