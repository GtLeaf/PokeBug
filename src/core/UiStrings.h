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
static constexpr const char* INFO_TITLE       = "Status";         // 状态标题
static constexpr const char* ATTR_TITLE       = "Attributes";     // 属性标题
static constexpr const char* INFO_NAV         = "A:Back B:Next";  // 导航提示：A返回 B下一页
static constexpr const char* INFO_NAV_SHORT   = "B:Next";         // 紧凑导航提示
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
static constexpr const char* MENU_SOCIAL      = "Social";      // 社交
static constexpr const char* MENU_EXPLORE     = "Explore";     // 探索
static constexpr const char* MENU_SETTINGS    = "Settings";    // 设置
static constexpr const char* MENU_DEBUG       = "Debug";       // 调试
static constexpr const char* MENU_CUP         = "Cup";         // 甲虫杯
static constexpr const char* MENU_WOOD        = "Wood";        // 腐木
static constexpr const char* MENU_BOWL        = "Bowl";        // 食物盘
static constexpr const char* MENU_SLEEP       = "Sleep";       // 睡觉
static constexpr const char* CARE_ITEM_NOT_NEEDED = "Not needed yet."; // 暂不需要这个

// Sleep 提示
static constexpr const char* SLEEP_TOO_EARLY  = "Too early to sleep.";          // 太早了，不能睡觉
static constexpr const char* SLEEP_BEETLE_DIED = "Beetle has died.";            // 甲虫已死亡
static constexpr const char* SLEEP_CONFIRM    = "Fast-forward to\n6:00 AM?";    // 将时间前进至早上6:00？
static constexpr const char* SLEEP_NAV        = "A:Yes  B:No";                   // A确认 B取消
static constexpr const char* SLEEP_GOOD_MORNING = "Good morning!";               // 早上好

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
static constexpr const char* HOLD_AB_RESET    = "Press A+B";          // 同时按A+B重置

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
static constexpr const char* GIFT_RECEIVE_TITLE = "GIFT";             // 礼物
static constexpr const char* GIFT_WAITING       = "RECEIVING";        // 等待礼物
static constexpr const char* GIFT_SENDING       = "SENDING";          // 发送礼物
static constexpr const char* GIFT_NO_FOOD       = "No food selected."; // 无可发送食物
static constexpr const char* GIFT_SENT          = "Gift sent!";       // 礼物已发送
static constexpr const char* GIFT_SEND_FAILED   = "Gift failed.";     // 礼物发送失败
static constexpr const char* GIFT_RECEIVED      = "Gift received!";   // 收到礼物
static constexpr const char* GIFT_UNSUPPORTED   = "Unsupported gift."; // 暂不支持

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

// 杯赛入口条件提示
static constexpr const char* CUP_NEED_ADULT    = "Only adult beetles\ncan join the cup.";
static constexpr const char* CUP_BEETLE_DIED   = "Your beetle\nhas passed away.";
static constexpr const char* CUP_NEED_HUNGER   = "Hunger too low.\nFeed first.";
static constexpr const char* CUP_NOT_OPEN      = "Registration is\nclosed.";

// 探索模式（ExploreScene）
static constexpr const char* EXPLORE_END              = "Explore End";
static constexpr const char* EXPLORE_RESULT_NAV       = "A:Continue B:Return";
static constexpr const char* EXPLORE_SAP_PLUS         = "Sap +%d";
static constexpr const char* EXPLORE_FOOD_SOURCE_PLUS = "Food source +%d";
static constexpr const char* EXPLORE_ROTTEN_WOOD_PLUS = "Sap +2";
static constexpr const char* EXPLORE_WOOD_FULL_SAP    = "Sap +1";
static constexpr const char* EXPLORE_GOLDEN_SAP       = "Golden sap +5";
static constexpr const char* EXPLORE_RESIN_CRYSTAL    = "Resin crystal +3 sap";
static constexpr const char* EXPLORE_BUTTERFLY_GUIDES = "Butterfly guides...";
static constexpr const char* EXPLORE_EXTRA_SAP        = "Extra sap +1";
static constexpr const char* EXPLORE_MUSHROOMS        = "Mushrooms +3 sap";
static constexpr const char* EXPLORE_HONEYDEW_SPRING  = "Honeydew spring +6";
static constexpr const char* EXPLORE_PHANTOM_BLESSING = "Phantom beetle blessing!";
static constexpr const char* EXPLORE_NOTHING_HAPPENED = "Nothing happened...";
static constexpr const char* EXPLORE_VICTORY_SAP      = "Victory! Sap +%d";
static constexpr const char* EXPLORE_SPI_PLUS         = "Ability improved";
static constexpr const char* EXPLORE_NO_PENALTY       = "No penalty";
static constexpr const char* EXPLORE_DEFEATED         = "Defeated...";
static constexpr const char* EXPLORE_FLED             = "Fled...";
static constexpr const char* EXPLORE_SAFE_RETURN      = "Safe return";
static constexpr const char* EXPLORE_IN_PROGRESS      = "Exploring...";
static constexpr const char* EXPLORE_TIME_REMAIN      = "Exploring %lus";
static constexpr const char* EXPLORE_NAV_BACK_RELEASE = "B:Back A:Release";
static constexpr const char* EXPLORE_NEED_ADULT       = "Only adult beetles\ncan explore.";
static constexpr const char* EXPLORE_BEETLE_DIED      = "Your beetle\nhas passed away.";
static constexpr const char* EXPLORE_NEED_HUNGER      = "Hunger too low.\nFeed first.";
static constexpr const char* EXPLORE_MOT_LOW          = "MOT too low.\nCheer up first.";
static constexpr const char* EXPLORE_DAILY_LIMIT      = "Daily limit reached.";
static constexpr const char* EXPLORE_NIGHT_FORBIDDEN  = "Too dangerous\nat night.";
static constexpr const char* EXPLORE_ROUND_FMT        = "Round %d/3";
static constexpr const char* EXPLORE_NEXT_RETURN      = "A:Next  B:Return";
static constexpr const char* EXPLORE_COMPLETE         = "Explore Complete";
static constexpr const char* EXPLORE_LEFT             = "Left...";
static constexpr const char* EXPLORE_TOO_YOUNG_FIGHT  = "Too young to fight.";
static constexpr const char* EXPLORE_MOT_ZERO         = "MOT = 0";
static constexpr const char* EXPLORE_PICNIC_CRUMBS    = "Picnic crumbs! Sap +%d";
static constexpr const char* EXPLORE_SPRINKLER        = "Sprinkler shower! +1";
static constexpr const char* EXPLORE_MORNING_DEW      = "Morning dew! +2";
static constexpr const char* EXPLORE_EARTHWORM        = "Earthworm digs sap +1";
static constexpr const char* EXPLORE_RAINBOW          = "Rainbow! Lucky sap +3";
static constexpr const char* EXPLORE_DANDELION        = "Dandelion fluff... +1";
static constexpr const char* EXPLORE_SUN_BARK         = "Sun-baked bark! Sap +1";
static constexpr const char* EXPLORE_ANT_TRAIL        = "Ant trail! Sap +2";
static constexpr const char* EXPLORE_PINECONE         = "Pinecone rolls... +1";
static constexpr const char* EXPLORE_CRACKED_STUMP    = "Cracked stump! Sap +2";
static constexpr const char* EXPLORE_LIZARD           = "Lizard sunbath! Sap +2";
static constexpr const char* EXPLORE_CAT_TRACKS       = "Mountain cat tracks...";
static constexpr const char* EXPLORE_MOSS_CARPET      = "Moss carpet! Sap +2";
static constexpr const char* EXPLORE_FIREFLIES        = "Fireflies! Sap +3";
static constexpr const char* EXPLORE_BULLFROG         = "Bullfrog croaks... +1";
static constexpr const char* EXPLORE_FAIRY_RING       = "Fairy ring! Sap +3";
static constexpr const char* EXPLORE_DRIFTWOOD        = "Driftwood ashore! +1";
static constexpr const char* EXPLORE_WATER_STRIDER    = "Water strider! +2";
static constexpr const char* EXPLORE_ANCIENT_RESIN    = "Ancient resin! Sap +4";
static constexpr const char* EXPLORE_DEER_MUSHROOM    = "Antler mushroom! +2";
static constexpr const char* EXPLORE_DEADWOOD_BEETLES = "Deadwood troop! +3";
static constexpr const char* EXPLORE_GLOW_MOSS        = "Glow moss! Sap +5";
static constexpr const char* EXPLORE_HEART_OF_ROT     = "Heart of rot! Sap +2";
static constexpr const char* EXPLORE_OLD_PHANTOM      = "Old tree phantom! +6";

// 腐木（Wood）
static constexpr const char* WOOD_NONE          = "None";
static constexpr const char* WOOD_PLACED        = "Wood placed!";
static constexpr const char* WOOD_NEED_ROTTEN   = "Wood locked.";
static constexpr const char* EXPLORE_CONTINUE         = "A:Continue";
static constexpr const char* EXPLORE_BEETLE_LABEL     = "Beetle:%s";
static constexpr const char* EXPLORE_FIGHT            = "A:Fight";
static constexpr const char* EXPLORE_FLEE             = "B:Flee";
static constexpr const char* EXPLORE_ACCEPT           = "A:Accept";
static constexpr const char* EXPLORE_LEAVE            = "B:Leave";
static constexpr const char* EXPLORE_CANT_FLEE        = "Can't flee";
static constexpr const char* EXPLORE_RELEASE_CONFIRM  = "Release this beetle?";
static constexpr const char* EXPLORE_RELEASE_EGG      = "It will leave an egg...";
static constexpr const char* EXPLORE_CONFIRM          = "A:Confirm";
static constexpr const char* EXPLORE_CANCEL           = "B:Cancel";

// 甲虫杯（CupScene）
static constexpr const char* CUP_SEASON_TITLE         = "Season %u Cup";
static constexpr const char* CUP_STARTING             = "Cup is starting!";
static constexpr const char* CUP_NAV_JOIN_QUIT        = "A:Join  B:Quit";
static constexpr const char* CUP_BRACKET              = "Bracket";
static constexpr const char* CUP_YOU_VS               = "You VS %s";
static constexpr const char* CUP_VS                   = " VS ";
static constexpr const char* CUP_START_ROUND          = "A:Start Round 1";
static constexpr const char* CUP_ROUND_QUARTER        = "Quarter";
static constexpr const char* CUP_ROUND_SEMI           = "Semi";
static constexpr const char* CUP_ROUND_FINAL          = "Final";
static constexpr const char* CUP_OPPONENT             = "Opponent: %s";
static constexpr const char* CUP_BEETLE_LABEL         = "Beetle: %s";
static constexpr const char* CUP_BATTLE               = "A:Battle";
static constexpr const char* CUP_REWARD_ISSUED        = "Reward issued";
static constexpr const char* CUP_DID_NOT_JOIN         = "Did not join";
static constexpr const char* CUP_BACK                 = "A:Back";
static constexpr const char* CUP_CHAMPION             = "Champion!";
static constexpr const char* CUP_RUNNER_UP            = "Runner-up";
static constexpr const char* CUP_TOP_FOUR             = "Top 4";
static constexpr const char* CUP_TOP_EIGHT            = "Top 8";

// Social / Fight / Gift 子菜单
static constexpr const char* MENU_SOCIAL_GIFT         = "Gift";
static constexpr const char* MENU_SOCIAL_FIGHT        = "Fight";
static constexpr const char* MENU_GIFT_SEND_FOOD      = "Send Food";
static constexpr const char* MENU_GIFT_RECEIVE_FOOD   = "Receive Food";
static constexpr const char* MENU_FIGHT_CUP           = "Beetle Cup";
static constexpr const char* MENU_FIGHT_CREATE        = "Create Room";
static constexpr const char* MENU_FIGHT_SEARCH        = "Search Room";

// Debug 子菜单
static constexpr const char* MENU_DEBUG_NPC             = "VS NPC";

// 孵化提示
static constexpr const char* HATCH_HINT_VIGOROUS = "Full of energy!";
static constexpr const char* HATCH_HINT_HEALTHY  = "Looks healthy";
static constexpr const char* HATCH_HINT_QUIET    = "A bit quiet...";
static constexpr const char* HATCH_HINT_WEAK     = "Seems fragile";

} // namespace UiStrings
