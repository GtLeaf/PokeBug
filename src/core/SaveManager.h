#pragma once
#include "../game/Bug.h"

// ============================================================
// 存档管理器 — 封装 Preferences (NVS) 存储
// ============================================================
class SaveManager {
public:
    static SaveManager& ins();

    // 加载/保存 独角仙
    bool load(Bug& bug);
    bool save(const Bug& bug);

    // 清除所有存档
    void clear();

    // 检查是否存在存档
    bool hasSave() const;

    // 保存/加载全局设置
    bool saveSettings(float fontScale, uint8_t brightness, float gameSpeed,
                      uint8_t idleTimeout, uint8_t mainSceneBg,
                      uint8_t woodStyle, uint8_t bowlStyle, uint8_t foodStyle);
    bool loadSettings(float& fontScale, uint8_t& brightness, float& gameSpeed,
                      uint8_t& idleTimeout, uint8_t& mainSceneBg,
                      uint8_t& woodStyle, uint8_t& bowlStyle, uint8_t& foodStyle);

    // 全局杯赛数据（跨虫持久）
    bool saveCupGlobal(uint16_t season, uint32_t lastCupGameTime, uint8_t state);
    bool loadCupGlobal(uint16_t& season, uint32_t& lastCupGameTime, uint8_t& state);
    void clearCupGlobal();

private:
    SaveManager() = default;

    static constexpr const char* NAMESPACE = "pokebug";
    static constexpr const char* KEY_BUG   = "bug";
    static constexpr const char* KEY_FONT  = "fontscale";
    static constexpr const char* KEY_BRI   = "brightness";
    static constexpr const char* KEY_SPEED = "gamespeed";
    static constexpr const char* KEY_IDLE  = "idletime";
    static constexpr const char* KEY_BG    = "mainbg";
    static constexpr const char* KEY_WOOD  = "woodstyle";
    static constexpr const char* KEY_BOWL  = "bowlstyle";
    static constexpr const char* KEY_FOOD  = "foodstyle";
    static constexpr const char* KEY_VER   = "savever";
    static constexpr const char* KEY_CUP_SEASON = "cup_season";
    static constexpr const char* KEY_CUP_TIME   = "last_cup_time";
    static constexpr const char* KEY_CUP_GAME_TIME = "cup_game_time";
    static constexpr const char* KEY_CUP_STATE  = "cup_state";
    static constexpr uint8_t SAVE_VERSION = 8;

    bool isSaving = false;  // 防并发写入锁
};
