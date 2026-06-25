#pragma once
#include <Arduino.h>

// ============================================================
// NPC 数据（探索 & 甲虫杯共用）
// 12 个 NPC，分 4 档：新手(3)/普通(4)/老手(3)/传说(3)
// 名字、甲虫名、遭遇/胜利/失败台词均存于 PROGMEM
// ============================================================

namespace NpcData {

enum class Tier : uint8_t {
    ROOKIE = 0,
    NORMAL,
    VETERAN,
    LEGEND,
    COUNT
};

static constexpr uint8_t ENTRY_COUNT = 12;

static constexpr const char* TIER_ROOKIE_NAME  = "Rookie";
static constexpr const char* TIER_NORMAL_NAME  = "Normal";
static constexpr const char* TIER_VETERAN_NAME = "Veteran";
static constexpr const char* TIER_LEGEND_NAME  = "Legend";
static constexpr const char* TIER_UNKNOWN_NAME = "?";
static constexpr uint8_t COUNT = ENTRY_COUNT;  // 兼容旧代码中的 NpcData::COUNT

struct NpcEntry {
    Tier tier;
    const char* name;       // 训练家名字
    const char* bugName;    // 甲虫名字
    const char* meet;       // 遭遇台词
    const char* win;        // 胜利台词（NPC 赢）
    const char* lose;       // 失败台词（玩家赢）
};

// 新手档
static const char NPC_ROOKIE_0_NAME[] PROGMEM = "Beetle Kid";
static const char NPC_ROOKIE_0_BUG[]  PROGMEM = "Tiny Horn";
static const char NPC_ROOKIE_0_MEET[] PROGMEM = "Mom says I can keep a beetle!";
static const char NPC_ROOKIE_0_WIN[]  PROGMEM = "Waaah... Tiny Horn, get up!";
static const char NPC_ROOKIE_0_LOSE[] PROGMEM = "Yay! Tiny Horn won!";

static const char NPC_ROOKIE_1_NAME[] PROGMEM = "Neighbor Mimi";
static const char NPC_ROOKIE_1_BUG[]  PROGMEM = "Flower";
static const char NPC_ROOKIE_1_MEET[] PROGMEM = "My Flower is super cute!";
static const char NPC_ROOKIE_1_WIN[]  PROGMEM = "Flower, don't be scared...";
static const char NPC_ROOKIE_1_LOSE[] PROGMEM = "Flower is the best!";

static const char NPC_ROOKIE_2_NAME[] PROGMEM = "Passerby Chai";
static const char NPC_ROOKIE_2_BUG[]  PROGMEM = "Chai Ball";
static const char NPC_ROOKIE_2_MEET[] PROGMEM = "Woof! I mean... walking my beetle.";
static const char NPC_ROOKIE_2_WIN[]  PROGMEM = "Chai Ball, stand up!";
static const char NPC_ROOKIE_2_LOSE[] PROGMEM = "Chai Ball is amazing! Woof!";

// 普通档
static const char NPC_NORMAL_0_NAME[] PROGMEM = "Beetle Taro";
static const char NPC_NORMAL_0_BUG[]  PROGMEM = "Horn Taro";
static const char NPC_NORMAL_0_MEET[] PROGMEM = "My Horn Taro won't lose!";
static const char NPC_NORMAL_0_WIN[]  PROGMEM = "Horn Taro, next time!";
static const char NPC_NORMAL_0_LOSE[] PROGMEM = "That's Horn Taro for you!";

static const char NPC_NORMAL_1_NAME[] PROGMEM = "Beetle Auntie";
static const char NPC_NORMAL_1_BUG[]  PROGMEM = "Iron Shell";
static const char NPC_NORMAL_1_MEET[] PROGMEM = "I've raised beetles 20 years!";
static const char NPC_NORMAL_1_WIN[]  PROGMEM = "Oh my, youngsters these days...";
static const char NPC_NORMAL_1_LOSE[] PROGMEM = "My beetle's still tough!";

static const char NPC_NORMAL_2_NAME[] PROGMEM = "Old Chen";
static const char NPC_NORMAL_2_BUG[]  PROGMEM = "Old Pal";
static const char NPC_NORMAL_2_MEET[] PROGMEM = "I walk my beetle every day.";
static const char NPC_NORMAL_2_WIN[]  PROGMEM = "Getting old...";
static const char NPC_NORMAL_2_LOSE[] PROGMEM = "Old Pal still got it?";

static const char NPC_NORMAL_3_NAME[] PROGMEM = "Uncle Yamada";
static const char NPC_NORMAL_3_BUG[]  PROGMEM = "Yamada-go";
static const char NPC_NORMAL_3_MEET[] PROGMEM = "This one's wild from the mountains!";
static const char NPC_NORMAL_3_WIN[]  PROGMEM = "Wild lost... unbelievable.";
static const char NPC_NORMAL_3_LOSE[] PROGMEM = "Wild is strong!";

// 老手档
static const char NPC_VETERAN_0_NAME[] PROGMEM = "Beetle King Meng";
static const char NPC_VETERAN_0_BUG[]  PROGMEM = "Iron Horn";
static const char NPC_VETERAN_0_MEET[] PROGMEM = "I'll show you a real beetle!";
static const char NPC_VETERAN_0_WIN[]  PROGMEM = "Hmph... bad day today.";
static const char NPC_VETERAN_0_LOSE[] PROGMEM = "This is the power gap!";

static const char NPC_VETERAN_1_NAME[] PROGMEM = "Deep Hunter";
static const char NPC_VETERAN_1_BUG[]  PROGMEM = "Shadow Horn";
static const char NPC_VETERAN_1_MEET[] PROGMEM = "I chased it for three days.";
static const char NPC_VETERAN_1_WIN[]  PROGMEM = "Interesting... next time.";
static const char NPC_VETERAN_1_LOSE[] PROGMEM = "Your beetle's not bad.";

static const char NPC_VETERAN_2_NAME[] PROGMEM = "Horn Sage";
static const char NPC_VETERAN_2_BUG[]  PROGMEM = "Horn Lord";
static const char NPC_VETERAN_2_MEET[] PROGMEM = "Train more and come back.";
static const char NPC_VETERAN_2_WIN[]  PROGMEM = "...Impossible.";
static const char NPC_VETERAN_2_LOSE[] PROGMEM = "You need more training.";

// 传说档
static const char NPC_LEGEND_0_NAME[] PROGMEM = "Dragon Beetle Ryo";
static const char NPC_LEGEND_0_BUG[]  PROGMEM = "Dragon Horn";
static const char NPC_LEGEND_0_MEET[] PROGMEM = "Your beetle... has dragon aura.";
static const char NPC_LEGEND_0_WIN[]  PROGMEM = "I see... your potential.";
static const char NPC_LEGEND_0_LOSE[] PROGMEM = "Dragons don't lose to mortals.";

static const char NPC_LEGEND_1_NAME[] PROGMEM = "Black Horn Sage";
static const char NPC_LEGEND_1_BUG[]  PROGMEM = "Black Horn";
static const char NPC_LEGEND_1_MEET[] PROGMEM = "A century of training for this.";
static const char NPC_LEGEND_1_WIN[]  PROGMEM = "Once in a century...";
static const char NPC_LEGEND_1_LOSE[] PROGMEM = "A century's power is no joke.";

static const char NPC_LEGEND_2_NAME[] PROGMEM = "Forest Guardian";
static const char NPC_LEGEND_2_BUG[]  PROGMEM = "Forest Horn";
static const char NPC_LEGEND_2_MEET[] PROGMEM = "The forest watches this fight.";
static const char NPC_LEGEND_2_WIN[]  PROGMEM = "The forest remembers you.";
static const char NPC_LEGEND_2_LOSE[] PROGMEM = "The forest says... not yet.";

static const NpcEntry ENTRIES[] PROGMEM = {
    { Tier::ROOKIE,  NPC_ROOKIE_0_NAME,  NPC_ROOKIE_0_BUG,  NPC_ROOKIE_0_MEET,  NPC_ROOKIE_0_WIN,  NPC_ROOKIE_0_LOSE },
    { Tier::ROOKIE,  NPC_ROOKIE_1_NAME,  NPC_ROOKIE_1_BUG,  NPC_ROOKIE_1_MEET,  NPC_ROOKIE_1_WIN,  NPC_ROOKIE_1_LOSE },
    { Tier::ROOKIE,  NPC_ROOKIE_2_NAME,  NPC_ROOKIE_2_BUG,  NPC_ROOKIE_2_MEET,  NPC_ROOKIE_2_WIN,  NPC_ROOKIE_2_LOSE },
    { Tier::NORMAL,  NPC_NORMAL_0_NAME,  NPC_NORMAL_0_BUG,  NPC_NORMAL_0_MEET,  NPC_NORMAL_0_WIN,  NPC_NORMAL_0_LOSE },
    { Tier::NORMAL,  NPC_NORMAL_1_NAME,  NPC_NORMAL_1_BUG,  NPC_NORMAL_1_MEET,  NPC_NORMAL_1_WIN,  NPC_NORMAL_1_LOSE },
    { Tier::NORMAL,  NPC_NORMAL_2_NAME,  NPC_NORMAL_2_BUG,  NPC_NORMAL_2_MEET,  NPC_NORMAL_2_WIN,  NPC_NORMAL_2_LOSE },
    { Tier::NORMAL,  NPC_NORMAL_3_NAME,  NPC_NORMAL_3_BUG,  NPC_NORMAL_3_MEET,  NPC_NORMAL_3_WIN,  NPC_NORMAL_3_LOSE },
    { Tier::VETERAN, NPC_VETERAN_0_NAME, NPC_VETERAN_0_BUG, NPC_VETERAN_0_MEET, NPC_VETERAN_0_WIN, NPC_VETERAN_0_LOSE },
    { Tier::VETERAN, NPC_VETERAN_1_NAME, NPC_VETERAN_1_BUG, NPC_VETERAN_1_MEET, NPC_VETERAN_1_WIN, NPC_VETERAN_1_LOSE },
    { Tier::VETERAN, NPC_VETERAN_2_NAME, NPC_VETERAN_2_BUG, NPC_VETERAN_2_MEET, NPC_VETERAN_2_WIN, NPC_VETERAN_2_LOSE },
    { Tier::LEGEND,  NPC_LEGEND_0_NAME,  NPC_LEGEND_0_BUG,  NPC_LEGEND_0_MEET,  NPC_LEGEND_0_WIN,  NPC_LEGEND_0_LOSE },
    { Tier::LEGEND,  NPC_LEGEND_1_NAME,  NPC_LEGEND_1_BUG,  NPC_LEGEND_1_MEET,  NPC_LEGEND_1_WIN,  NPC_LEGEND_1_LOSE },
    { Tier::LEGEND,  NPC_LEGEND_2_NAME,  NPC_LEGEND_2_BUG,  NPC_LEGEND_2_MEET,  NPC_LEGEND_2_WIN,  NPC_LEGEND_2_LOSE },
};

// 探索模式档位权重：新手 40% / 普通 35% / 老手 20% / 传说 5%
static constexpr uint8_t EXPLORER_TIER_WEIGHTS[4] = { 40, 35, 20, 5 };
// 甲虫杯档位权重：新手 10% / 普通 30% / 老手 40% / 传说 20%
static constexpr uint8_t CUP_TIER_WEIGHTS[4]      = { 10, 30, 40, 20 };

inline const char* tierName(Tier t) {
    switch (t) {
        case Tier::ROOKIE:  return TIER_ROOKIE_NAME;
        case Tier::NORMAL:  return TIER_NORMAL_NAME;
        case Tier::VETERAN: return TIER_VETERAN_NAME;
        case Tier::LEGEND:  return TIER_LEGEND_NAME;
        default: return TIER_UNKNOWN_NAME;
    }
}

inline uint16_t tierColor(Tier t) {
    switch (t) {
        case Tier::ROOKIE:  return 0x07E0; // green
        case Tier::NORMAL:  return 0xFFE0; // yellow
        case Tier::VETERAN: return 0xFBE0; // orange
        case Tier::LEGEND:  return 0xF800; // red
        default: return 0xFFFF;
    }
}

} // namespace NpcData
