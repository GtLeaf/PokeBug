#pragma once
#include "../core/Scene.h"
#include "../core/GameEngine.h"
#include "../game/NpcGenerator.h"
#include "../assets/NpcData.h"

// 探索模式场景
class ExploreScene : public Scene {
public:
    ExploreScene() = default;

    void onEnter() override;
    void onExit() override;
    SceneID update() override;
    void render() override;
    bool onButton(const ButtonEvent& ev) override;

private:
    enum class State {
        EXPLORING,        // 轮次开始，先展示场景再触发事件
        EVENT_REVEAL,     // 事件出现前的轻微晃动
        EVENT_POPUP,      // 普通事件弹窗
        NPC_PROMPT,       // NPC 遭遇：迎战/逃跑
        FINAL_SUMMARY,    // 3 轮完成或提前结束
        RETURNING,        // 返回培养缸
    };
    State state = State::EXPLORING;

    // 探索参数
    static constexpr uint8_t MAX_EXPLORE_ROUNDS = 3;
    static constexpr int PROCEDURAL_GROUND_Y = 98;

    uint32_t roundStartedAtMs = 0;
    uint32_t revealStartedAtMs = 0;
    State pendingRevealState = State::EVENT_POPUP;

    // 轮次与收益
    uint8_t currentRound = 1;
    int totalSapGain = 0;
    bool finalRecorded = false;

    // 当前事件
    enum class EventType {
        SAP,         // 发现树汁
        FOOD_SOURCE, // 发现食物源
        WOOD,        // 发现木质环境资源，奖励树汁
        NPC,         // 遭遇对手
        RARE,        // 稀有事件
        NOTHING,     // 无事发生
    };
    EventType eventType = EventType::NOTHING;
    int eventValue = 0;       // 资源数量
    int rareSubType = 0;      // 稀有事件子类型 0-5

    // NPC 遭遇
    NpcCombatant npc;
    char npcName[16];
    char npcBugName[16];
    char npcMeetLine[48];

    // UI 文本
    char resultLine1[48];
    char resultLine2[48];

    // 内部方法
    void startNewSession();
    void restoreSession();
    void saveSession();
    void clearSession();
    void startRoundWait(uint32_t now);
    void beginEventReveal(uint32_t now, State nextState);
    int revealShakeOffset(uint32_t now) const;
    void triggerEvent(uint32_t now);
    void advanceRoundOrFinish();
    void enterFinalSummary(const char* line1, const char* line2 = nullptr);
    void applyEventReward(bool flee = false);
    void startNpcBattle();
    void applyNpcBattleResult(const NpcBattleResult& res);
    void drawExploring(int shakeX = 0, bool showProgressText = true);
    void drawPopup();
    void drawNpcPrompt();
    void drawResult();
    void drawSkyAndGround(int shakeX = 0);

    static bool sSessionActive;
    static uint8_t sCurrentRound;
    static int sTotalSapGain;
    static bool sFinalRecorded;
};
