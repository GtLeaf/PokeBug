#pragma once

// ============================================================
// UI 文案常量集合
// 所有用户可见的字符串统一放在这里，便于后续做多语言切换。
// 当前为英文版本。
// ============================================================

namespace UiStrings {

// 通用
static constexpr const char* BACK       = "Back";      // 返回
static constexpr const char* PLACE      = "Place";     // 放置
static constexpr const char* TYPE       = "Type";      // 类型
static constexpr const char* BG         = "BG";        // 背景

// Info 场景（信息页 / 属性页）
static constexpr const char* INFO_TITLE       = "INFO";           // 信息标题
static constexpr const char* INFO_NAV         = "A:Back B:Next";  // 导航提示：A返回 B下一页
static constexpr const char* RECORD_TITLE     = "Battle Record";  // 对战记录标题
static constexpr const char* GEN              = "Gen";              // 世代
static constexpr const char* MOT              = "MOT";              // 心情（积极性）
static constexpr const char* HUN              = "HUN";              // 饥饿度
static constexpr const char* DROP             = "Drop";             // 水滴（食物类型）
static constexpr const char* TOTAL            = "Total";            // 总计
static constexpr const char* WINS             = "Wins";             // 胜场
static constexpr const char* LOSSES           = "Losses";           // 败场
static constexpr const char* RATE             = "Rate";             // 胜率

// Menu 场景（主菜单）
static constexpr const char* MENU_INFO        = "Info";        // 信息
static constexpr const char* MENU_FEED        = "Food";        // 食物
static constexpr const char* MENU_BOX         = "Box";         // 箱子/背包
static constexpr const char* MENU_FIGHT       = "Fight";       // 对战
static constexpr const char* MENU_EXPLORE     = "Explore";     // 探索
static constexpr const char* MENU_SETTINGS    = "Settings";    // 设置
static constexpr const char* MENU_WOOD        = "Wood";        // 腐木
static constexpr const char* MENU_BOWL        = "Bowl";        // 食物盘

// Settings 场景（设置界面）
static constexpr const char* SET_BRIGHTNESS   = "Brightness";    // 亮度
static constexpr const char* SET_FONT_SIZE    = "Font Size";       // 字体大小
static constexpr const char* SET_GAME_SPEED   = "Game Speed";      // 游戏速度
static constexpr const char* SET_IDLE_TIME    = "Idle Time";       // 空闲时间（自动降低背光）
static constexpr const char* SET_RESET_SAVE   = "Reset Save";      // 重置存档
static constexpr const char* RESET_CONFIRM    = "Reset save?";     // 确认重置？
static constexpr const char* RESET_NAV        = "A:Yes  B/Long:No"; // A确认 B长按取消

static constexpr const char* IDLE_30S         = "30s";     // 30秒
static constexpr const char* IDLE_1M          = "1m";      // 1分钟
static constexpr const char* IDLE_2M          = "2m";      // 2分钟
static constexpr const char* IDLE_5M          = "5m";      // 5分钟
static constexpr const char* IDLE_NEVER       = "Never";   // 永不

// Terrarium 场景（培养缸主界面）
static constexpr const char* DIED             = "DIED";              // 死亡
static constexpr const char* HOLD_AB_RESET    = "Hold A+B 3s";        // 长按A+B 3秒重置

// Lobby 场景（对战大厅）
static constexpr const char* LOBBY_ROOM       = "ROOM";              // 房间
static constexpr const char* LOBBY_WAITING    = "WAITING";           // 等待中
static constexpr const char* LOBBY_SCANNING   = "SCANNING";          // 扫描中
static constexpr const char* LOBBY_NO_ROOMS   = "NO ROOMS";          // 无房间
static constexpr const char* LOBBY_JOINING    = "JOINING";           // 加入中
static constexpr const char* LOBBY_SELECT_ROOM = "SELECT ROOM";       // 选择房间
static constexpr const char* MSG_NO_ONE_JOINED = "NO ONE JOINED";     // 无人加入
static constexpr const char* MSG_REJECTED      = "REJECTED";          // 被拒绝
static constexpr const char* MSG_JOIN_TIMEOUT  = "JOIN TIMEOUT";      // 加入超时

// Battle 场景（对战界面）
static constexpr const char* BATTLE_SEARCHING = "Searching...";    // 搜索中...
static constexpr const char* BATTLE_ROUND     = "ROUND";           // 回合
static constexpr const char* BATTLE_HP        = "HP";              // 生命值
static constexpr const char* BATTLE_MOT       = "MOT";             // 心情（对战斗志）
static constexpr const char* BATTLE_PRESS_A   = "Press A return";  // 按A返回

// 气质（Temperament）—— 卵期交互行为决定的终身属性倾向
static constexpr const char* TEMP_SWIFT       = "Swift";       // 迅捷：SPD提升，SIZ降低
static constexpr const char* TEMP_RESILIENT   = "Resilient";   // 韧甲：END提升，SPI降低
static constexpr const char* TEMP_GIANT       = "Giant";       // 巨体：SIZ提升，SPD降低
static constexpr const char* TEMP_BRUTE       = "Brute";       // 蛮力：STR提升，END降低
static constexpr const char* TEMP_BALANCED    = "Balanced";    // 均衡：无增益无减益
static constexpr const char* TEMP_SPIRIT      = "Spirit";      // 灵心：SPI提升，STR降低

// 孵化提示
static constexpr const char* HATCH_HINT_VIGOROUS = "Full of energy!";
static constexpr const char* HATCH_HINT_HEALTHY  = "Looks healthy";
static constexpr const char* HATCH_HINT_QUIET    = "A bit quiet...";
static constexpr const char* HATCH_HINT_WEAK     = "Seems fragile";

} // namespace UiStrings
