#pragma once
#include <M5Unified.h>

// 硬件抽象层 — 封装 M5Unified
// 单例，通过 Hal::ins() 访问
class Hal {
public:
    static Hal& ins();

    // 初始化硬件（在 setup() 中调用一次）
    bool begin();

    // ========== 显示 ==========
    LGFX_Sprite& canvas();           // 离屏画布 240×135 RGB565
    void flush();                     // 推送画布到屏幕
    void setBrightness(uint8_t v);
    uint8_t getBrightness() const { return brightness; }   // 0-255
    void setIdleBrightness(bool idle);  // idle 时降低背光

    // ========== 按键 ==========
    bool btnA_raw() const;  // KEY1 原始状态
    bool btnB_raw() const;  // KEY2 原始状态（兼容 BtnPWR）

    // ========== 电源 ==========
    void lightSleep(uint32_t seconds);
    int batteryLevel();     // 0-100

    // ========== 时间（毫秒）==========
    uint32_t millis() const;

    // ========== IMU ==========
    void updateIMU();
    void getAccel(float& x, float& y, float& z);
    float getAccelMagnitude() const;  // 加速度模长（g）
    bool isShaken();                  // 是否发生剧烈摇晃（>2.0g，带 500ms 冷却）

    // PokeBug 使用横屏 240×135
    static constexpr int DISPLAY_W = 240;
    static constexpr int DISPLAY_H = 135;

private:
    Hal() = default;
    LGFX_Sprite sprite;
    bool initialized = false;

    float accX = 0, accY = 0, accZ = 0;
    uint8_t brightness = 128;

    uint32_t lastShakeTime = 0;
    bool batteryFilterReady = false;
    float filteredBatteryLevel = 0.0f;
    int displayedBatteryLevel = -1;
    uint32_t lastBatterySampleMs = 0;
    static constexpr float SHAKE_THRESHOLD_G = 2.0f;
    static constexpr uint32_t SHAKE_COOLDOWN_MS = 500;
    static constexpr uint32_t BATTERY_SAMPLE_MS = 15000;
    static constexpr float BATTERY_EMA_ALPHA = 0.20f;
    static constexpr int BATTERY_MAX_RISE_PER_SAMPLE = 1;
};
