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
        EXPLORING,        // 自由移动，倒计时触发事件
        EVENT_POPUP,      // 普通事件弹窗
        NPC_PROMPT,       // NPC 遭遇：迎战/逃跑
        RELEASE_CONFIRM,  // 放生确认
        RESULT,           // 事件/NPC 结果展示
        RETURNING,        // 返回培养缸
    };
    State state = State::EXPLORING;

    // 探索参数
    static constexpr uint32_t EXPLORE_DURATION_MS = 45000; // 45 秒探索时长
    static constexpr uint32_t EVENT_INTERVAL_MIN_MS = 5000;
    static constexpr uint32_t EVENT_INTERVAL_MAX_MS = 10000;
    static constexpr float TILT_THRESHOLD = 0.35f;
    static constexpr int BUG_Y = 88;
    static constexpr int MIN_X = 20;
    static constexpr int MAX_X = 220;

    // 甲虫在场景中的位置
    float bugX = 120.0f;
    bool faceRight = true;

    // 事件计时
    uint32_t exploreStartMs = 0;
    uint32_t nextEventMs = 0;
    uint32_t resultTimeoutMs = 0;

    // 当前事件
    enum class EventType {
        SAP,         // 发现树汁
        FOOD_SOURCE, // 发现食物源
        WOOD,        // 发现腐木
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
    bool canFlee = true;

    // UI 文本
    char resultLine1[48];
    char resultLine2[48];

    // 内部方法
    void resetEventTimer(uint32_t now);
    void triggerEvent(uint32_t now);
    void applyEventReward(bool flee = false);
    void startNpcBattle();
    void applyNpcBattleResult(const NpcBattleResult& res);
    void doRelease();
    void drawExploring();
    void drawPopup();
    void drawNpcPrompt();
    void drawReleaseConfirm();
    void drawResult();
    void drawBug(int x, int y, bool right, uint8_t palette);
    void drawSkyAndGround();
};
