#pragma once
#include <Arduino.h>
#include <cstdint>

// 甲虫欲望 — 由需求层驱动
enum class Desire : uint8_t {
    EAT = 0,     // 进食
    REST,        // 休息/睡觉
    WANDER,      // 漫游/探索
    STARE,       // 发呆/张望
    HIDE,        // 躲避/警戒
    COUNT
};

// 甲虫情绪 — 由主导需求映射，影响内心独白与行为倾向
enum class Mood : uint8_t {
    CALM = 0,    // 平静
    HUNGRY,      // 饥饿
    SLEEPY,      // 困倦
    ALERT,       // 警觉
    BORED,       // 无聊
    CURIOUS,     // 好奇
    ANGRY,       // 生气（被戳后）
    EXCITED,     // 兴奋（刚吃饱 / MOT高）
    COUNT
};

// 甲虫心智 — 基于需求-欲望-行为的轻量 AI
// 由 TerrariumScene 持有，每帧更新；不修改 Bug 状态，只读取。
class BugMind {
public:
    BugMind();

    // 每帧更新心智。foodAvailable = 盘中是否有食物且未吃完。
    void update(uint8_t hunger, uint8_t mot, bool isNight, bool woodPlaced,
                bool foodAvailable, uint32_t realNow);

    // 当前主导欲望
    Desire topDesire() const { return topDesire_; }
    // 当前情绪
    Mood mood() const { return mood_; }

    bool isDesiring(Desire d) const { return topDesire_ == d; }

    // 内心独白（静态字符串，供日志输出）
    const char* innerVoice() const;
    const char* moodName() const;
    const char* desireName() const;

    // 外部事件接口 — 在 TerrariumScene 中响应玩家交互时调用
    void onPoked(uint32_t realNow);
    void onShaken(uint32_t realNow);
    void onTilted(uint32_t realNow);
    void onAte(uint32_t realNow);
    void onRested(uint32_t realNow);

    // 重置活动计时（进入新状态时调用，防止无聊值累积）
    void resetActivityTimer(uint32_t realNow);

private:
    // 需求层：0-255，越高越紧迫
    uint8_t hungerNeed = 0;
    uint8_t energyNeed = 0;
    uint8_t safetyNeed = 0;
    uint8_t curiosityNeed = 0;

    // 欲望层：由需求计算出的分数
    uint8_t desireScores[(uint8_t)Desire::COUNT] = {0};

    Desire topDesire_ = Desire::STARE;
    Mood mood_ = Mood::CALM;

    uint32_t lastUpdateMs = 0;
    uint32_t lastActivityMs = 0;
    uint32_t alertUntilMs = 0;
    uint32_t pokedAtMs = 0;
    uint32_t shakeAtMs = 0;
    uint32_t tiltAtMs = 0;
    uint32_t ateAtMs = 0;

    static constexpr uint8_t INERTIA_BONUS = 22;      // 当前欲望的惯性加成
    static constexpr uint8_t SWITCH_MARGIN = 14;      // 新欲望必须明显更强才切换
    static constexpr uint8_t RANDOM_NOISE_MAX = 12;   // 欲望随机抖动

    void computeDesires(uint8_t hunger, uint8_t mot, bool isNight,
                        bool woodPlaced, bool foodAvailable, uint32_t realNow);
    void computeMood(uint8_t hunger, uint8_t mot, bool isNight);
    uint8_t clamp(uint16_t v) const { return v > 255 ? 255 : (uint8_t)v; }
};
