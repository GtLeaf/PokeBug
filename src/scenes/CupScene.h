#pragma once
#include "../core/Scene.h"
#include "../game/NpcGenerator.h"

// 甲虫杯场景：通知 → 对阵表 → 逐轮本地 NPC 对战 → 结算
class CupScene : public Scene {
public:
    CupScene() = default;

    void onEnter() override;
    void onExit() override;
    SceneID update() override;
    void render() override;
    bool onButton(const ButtonEvent& ev) override;

private:
    enum class State {
        NOTIFY,       // 杯赛开始通知
        BRACKET,      // 8 强对阵表
        ROUND_INTRO,  // 本轮对手介绍
        WAIT_BATTLE,  // 等待对战返回
        RESULT,       // 最终名次与奖励
    };
    State state = State::NOTIFY;

    // 杯赛静态状态（BattleScene 返回后重建场景时保留）
    static uint8_t sRound;        // 0=8强, 1=半决赛, 2=决赛
    static NpcCombatant sOpponents[7];
    static uint8_t sFinalRank;    // 1=冠军,2=亚军,4=四强,8=八强
    static bool sInitialized;
    static bool sParticipatedThisSeason;
    static bool sBeatLegend[3];   // 每轮是否击败传说 NPC

    // 超时
    uint32_t stateStartMs = 0;
    float bracketScroll = 0.0f;
    uint32_t bracketScrollLastMs = 0;
    static constexpr uint32_t NOTIFY_TIMEOUT_MS = 30000;

    void initCup();
    void advanceToNextRound(bool won);
    void finishCup(uint8_t rank);
    void applyCupRewards();
    void drawNotify();
    void drawBracket();
    void drawRoundIntro();
    void drawResult();
    void updateBracketScroll(int maxScroll);
    const char* rankText(uint8_t rank) const;
};
