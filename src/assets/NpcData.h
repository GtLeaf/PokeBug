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

struct NpcEntry {
    Tier tier;
    const char* name;       // 训练家名字
    const char* bugName;    // 甲虫名字
    const char* meet;       // 遭遇台词
    const char* win;        // 胜利台词（NPC 赢）
    const char* lose;       // 失败台词（玩家赢）
};

// 新手档
static const char NPC_ROOKIE_0_NAME[] PROGMEM = "虫虫小朋友";
static const char NPC_ROOKIE_0_BUG[]  PROGMEM = "小角角";
static const char NPC_ROOKIE_0_MEET[] PROGMEM = "妈妈说我可以养虫了！";
static const char NPC_ROOKIE_0_WIN[]  PROGMEM = "呜呜...小角角加油...";
static const char NPC_ROOKIE_0_LOSE[] PROGMEM = "耶！小角角赢了！";

static const char NPC_ROOKIE_1_NAME[] PROGMEM = "邻居小美";
static const char NPC_ROOKIE_1_BUG[]  PROGMEM = "花花";
static const char NPC_ROOKIE_1_MEET[] PROGMEM = "我的花花很可爱的~";
static const char NPC_ROOKIE_1_WIN[]  PROGMEM = "花花别怕...";
static const char NPC_ROOKIE_1_LOSE[] PROGMEM = "花花好棒！";

static const char NPC_ROOKIE_2_NAME[] PROGMEM = "路过的阿柴";
static const char NPC_ROOKIE_2_BUG[]  PROGMEM = "柴柴丸";
static const char NPC_ROOKIE_2_MEET[] PROGMEM = "汪！不对...我是来遛虫的";
static const char NPC_ROOKIE_2_WIN[]  PROGMEM = "柴柴丸快起来！";
static const char NPC_ROOKIE_2_LOSE[] PROGMEM = "柴柴丸太厉害了！汪！";

// 普通档
static const char NPC_NORMAL_0_NAME[] PROGMEM = "虫太郎";
static const char NPC_NORMAL_0_BUG[]  PROGMEM = "角太郎";
static const char NPC_NORMAL_0_MEET[] PROGMEM = "我的角太郎不会输！";
static const char NPC_NORMAL_0_WIN[]  PROGMEM = "角太郎...下次一定！";
static const char NPC_NORMAL_0_LOSE[] PROGMEM = "不愧是角太郎！";

static const char NPC_NORMAL_1_NAME[] PROGMEM = "甲虫阿姨";
static const char NPC_NORMAL_1_BUG[]  PROGMEM = "铁甲小宝";
static const char NPC_NORMAL_1_MEET[] PROGMEM = "阿姨养虫二十年了哦";
static const char NPC_NORMAL_1_WIN[]  PROGMEM = "哎哟，现在的年轻人...";
static const char NPC_NORMAL_1_LOSE[] PROGMEM = "阿姨的虫还硬朗着呢！";

static const char NPC_NORMAL_2_NAME[] PROGMEM = "公园老陈";
static const char NPC_NORMAL_2_BUG[]  PROGMEM = "老伙计";
static const char NPC_NORMAL_2_MEET[] PROGMEM = "每天公园遛虫，风雨无阻";
static const char NPC_NORMAL_2_WIN[]  PROGMEM = "老了老了...";
static const char NPC_NORMAL_2_LOSE[] PROGMEM = "老伙计还行吧？";

static const char NPC_NORMAL_3_NAME[] PROGMEM = "山田大叔";
static const char NPC_NORMAL_3_BUG[]  PROGMEM = "山田号";
static const char NPC_NORMAL_3_MEET[] PROGMEM = "这可是山里抓的纯野生！";
static const char NPC_NORMAL_3_WIN[]  PROGMEM = "野生的居然输了...";
static const char NPC_NORMAL_3_LOSE[] PROGMEM = "野生就是强！";

// 老手档
static const char NPC_VETERAN_0_NAME[] PROGMEM = "甲斗王阿猛";
static const char NPC_VETERAN_0_BUG[]  PROGMEM = "铁角将军";
static const char NPC_VETERAN_0_MEET[] PROGMEM = "让你见识什么叫真正的甲虫！";
static const char NPC_VETERAN_0_WIN[]  PROGMEM = "哼...今天状态不好";
static const char NPC_VETERAN_0_LOSE[] PROGMEM = "这就是实力差距！";

static const char NPC_VETERAN_1_NAME[] PROGMEM = "深山猎手";
static const char NPC_VETERAN_1_BUG[]  PROGMEM = "暗影角";
static const char NPC_VETERAN_1_MEET[] PROGMEM = "我在深山追了它三天三夜";
static const char NPC_VETERAN_1_WIN[]  PROGMEM = "有趣...下次不会了";
static const char NPC_VETERAN_1_LOSE[] PROGMEM = "你的虫也不错";

static const char NPC_VETERAN_2_NAME[] PROGMEM = "独角尊者";
static const char NPC_VETERAN_2_BUG[]  PROGMEM = "角尊";
static const char NPC_VETERAN_2_MEET[] PROGMEM = "修行不够，回去再练练";
static const char NPC_VETERAN_2_WIN[]  PROGMEM = "...居然";
static const char NPC_VETERAN_2_LOSE[] PROGMEM = "修行还远远不够";

// 传说档
static const char NPC_LEGEND_0_NAME[] PROGMEM = "虫神龙之介";
static const char NPC_LEGEND_0_BUG[]  PROGMEM = "龙角丸";
static const char NPC_LEGEND_0_MEET[] PROGMEM = "你的虫...有龙的气息";
static const char NPC_LEGEND_0_WIN[]  PROGMEM = "我看见了...你的潜力";
static const char NPC_LEGEND_0_LOSE[] PROGMEM = "龙不会输给凡人";

static const char NPC_LEGEND_1_NAME[] PROGMEM = "黑角仙人";
static const char NPC_LEGEND_1_BUG[]  PROGMEM = "黑角";
static const char NPC_LEGEND_1_MEET[] PROGMEM = "百年修行，只为等一个对手";
static const char NPC_LEGEND_1_WIN[]  PROGMEM = "百年一遇...有意思";
static const char NPC_LEGEND_1_LOSE[] PROGMEM = "百年修为，岂是儿戏";

static const char NPC_LEGEND_2_NAME[] PROGMEM = "森之守护者";
static const char NPC_LEGEND_2_BUG[]  PROGMEM = "森之角";
static const char NPC_LEGEND_2_MEET[] PROGMEM = "森林在看着这场战斗";
static const char NPC_LEGEND_2_WIN[]  PROGMEM = "森林记住了你的名字";
static const char NPC_LEGEND_2_LOSE[] PROGMEM = "森林说...还不到时候";

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

static constexpr uint8_t COUNT = 12;

// 探索遭遇概率
static constexpr uint8_t EXPLORER_TIER_WEIGHTS[4] = { 40, 35, 20, 5 };
// 杯赛 NPC 分布概率
static constexpr uint8_t CUP_TIER_WEIGHTS[4]      = { 10, 30, 40, 20 };

inline const char* tierName(Tier t) {
    switch (t) {
        case Tier::ROOKIE:  return "新手";
        case Tier::NORMAL:  return "普通";
        case Tier::VETERAN: return "老手";
        case Tier::LEGEND:  return "传说";
        default: return "?";
    }
}

inline uint16_t tierColor(Tier t) {
    switch (t) {
        case Tier::ROOKIE:  return 0x07E0; // 绿
        case Tier::NORMAL:  return 0xFFE0; // 黄
        case Tier::VETERAN: return 0xFD20; // 橙
        case Tier::LEGEND:  return 0xF800; // 红
        default: return 0xFFFF;
    }
}

} // namespace NpcData
