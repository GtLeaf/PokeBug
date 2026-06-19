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
    static constexpr uint8_t SAVE_VERSION = 7;

    bool isSaving = false;  // 防并发写入锁
};
