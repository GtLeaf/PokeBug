#include "Hal.h"
#include <cmath>
#include <Arduino.h>
#include <WiFi.h>

Hal& Hal::ins() {
    static Hal instance;
    return instance;
}

bool Hal::begin() {
    if (initialized) return true;

    Serial.println("[Hal] M5.begin() start");

    auto cfg = M5.config();
    cfg.internal_spk = false;   // 禁用扬声器，降低功耗/噪声
    cfg.internal_imu = true;    // PokeBug 需要 IMU
    cfg.internal_mic = false;
    cfg.external_rtc = false;

    M5.begin(cfg);
    Serial.println("[Hal] M5.begin() done");

    M5.Speaker.end();
    Serial.println("[Hal] speaker amp disabled");

    // 关闭 WiFi/BT 降低功耗（ESP-NOW 需要时再打开）
    WiFi.mode(WIFI_OFF);
    btStop();
    Serial.println("[Hal] WiFi/BT off");

    // 初始化 IMU
    if (M5.Imu.begin()) {
        Serial.println("[Hal] IMU initialized");
    } else {
        Serial.println("[Hal] IMU init failed");
    }

    delay(100);

    // 横屏：物理 135×240，逻辑 240×135
    sprite.setColorDepth(16);
    if (!sprite.createSprite(DISPLAY_W, DISPLAY_H)) {
        Serial.println("[Hal] ERROR: createSprite failed!");
        return false;
    }
    sprite.setSwapBytes(true);
    Serial.println("[Hal] Sprite created");

    M5.Display.setRotation(1);  // 横屏

    brightness = 128;
    M5.Display.setBrightness(brightness);

    initialized = true;
    Serial.println("[Hal] Init complete");
    return true;
}

LGFX_Sprite& Hal::canvas() {
    return sprite;
}

void Hal::flush() {
    sprite.pushSprite(&M5.Display, 0, 0);
}

void Hal::setBrightness(uint8_t v) {
    brightness = v;
    M5.Display.setBrightness(v);
}

void Hal::setIdleBrightness(bool idle) {
    static bool lastIdle = false;
    if (idle == lastIdle) return;
    lastIdle = idle;
    M5.Display.setBrightness(idle ? 16 : brightness);
}

bool Hal::btnA_raw() const {
    return M5.BtnA.isPressed();
}

bool Hal::btnB_raw() const {
    return M5.BtnB.isPressed() || M5.BtnPWR.isPressed();
}

void Hal::lightSleep(uint32_t seconds) {
    M5.Power.lightSleep(seconds);
}

int Hal::batteryLevel() {
    uint32_t now = millis();
    if (batteryFilterReady && now - lastBatterySampleMs < BATTERY_SAMPLE_MS) {
        return displayedBatteryLevel;
    }

    int raw = M5.Power.getBatteryLevel();
    lastBatterySampleMs = now;
    if (raw < 0) {
        return batteryFilterReady ? displayedBatteryLevel : raw;
    }
    if (raw > 100) raw = 100;

    auto charging = M5.Power.isCharging();
    bool isCharging = charging == m5::Power_Class::is_charging;
    if (!batteryFilterReady) {
        filteredBatteryLevel = (float)raw;
        displayedBatteryLevel = raw;
        batteryFilterReady = true;
        return displayedBatteryLevel;
    }

    float next = filteredBatteryLevel * (1.0f - BATTERY_EMA_ALPHA) +
                 (float)raw * BATTERY_EMA_ALPHA;
    int rounded = (int)(next + 0.5f);
    if (!isCharging && rounded > displayedBatteryLevel + BATTERY_MAX_RISE_PER_SAMPLE) {
        rounded = displayedBatteryLevel + BATTERY_MAX_RISE_PER_SAMPLE;
        next = (float)rounded;
    }
    if (rounded < 0) rounded = 0;
    if (rounded > 100) rounded = 100;

    filteredBatteryLevel = next;
    displayedBatteryLevel = rounded;
    return displayedBatteryLevel;
}

uint32_t Hal::millis() const {
    return M5.millis();
}

void Hal::updateIMU() {
    M5.Imu.getAccel(&accX, &accY, &accZ);
}

void Hal::getAccel(float& x, float& y, float& z) {
    x = accX; y = accY; z = accZ;
}

float Hal::getAccelMagnitude() const {
    return sqrtf(accX * accX + accY * accY + accZ * accZ);
}

bool Hal::isShaken() {
    uint32_t now = millis();
    if (now - lastShakeTime < SHAKE_COOLDOWN_MS) return false;

    float mag = getAccelMagnitude();
    if (mag > SHAKE_THRESHOLD_G) {
        lastShakeTime = now;
        return true;
    }
    return false;
}
