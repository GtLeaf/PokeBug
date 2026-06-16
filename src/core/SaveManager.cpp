#include "SaveManager.h"
#include <Preferences.h>
#include <Arduino.h>

SaveManager& SaveManager::ins() {
    static SaveManager instance;
    return instance;
}

bool SaveManager::load(Bug& bug) {
    Preferences prefs;
    if (!prefs.begin(NAMESPACE, true)) {
        Serial.println("[Save] NVS begin failed");
        return false;
    }

    uint8_t ver = prefs.getUChar(KEY_VER, 0);
    if (ver != SAVE_VERSION) {
        Serial.printf("[Save] Version mismatch: stored=%d expected=%d\n", ver, SAVE_VERSION);
        prefs.end();
        return false;
    }

    size_t len = prefs.getBytesLength(KEY_BUG);
    bool ok = false;
    if (len > 0 && len <= 64) {
        uint8_t buf[64];
        prefs.getBytes(KEY_BUG, buf, len);
        if (bug.load(buf, (uint16_t)len)) {
            Serial.printf("[Save] Bug loaded: %u bytes\n", len);
            ok = true;
        } else {
            Serial.println("[Save] Bug load failed");
        }
    } else {
        Serial.println("[Save] No bug save found");
    }

    prefs.end();
    return ok;
}

bool SaveManager::save(const Bug& bug) {
    if (isSaving) {
        Serial.println("[Save] Skip concurrent save");
        return false;
    }
    isSaving = true;

    Preferences prefs;
    if (!prefs.begin(NAMESPACE, false)) {
        Serial.println("[Save] NVS begin (write) failed");
        isSaving = false;
        return false;
    }

    prefs.putUChar(KEY_VER, SAVE_VERSION);

    uint8_t buf[64];
    uint16_t len = 0;
    bug.save(buf, len);
    if (len > 0) {
        prefs.putBytes(KEY_BUG, buf, len);
    }

    prefs.end();
    isSaving = false;
    Serial.printf("[Save] Bug saved: %u bytes\n", len);
    return true;
}

void SaveManager::clear() {
    Preferences prefs;
    if (prefs.begin(NAMESPACE, false)) {
        prefs.clear();
        prefs.end();
        Serial.println("[Save] All saves cleared");
    }
}

bool SaveManager::hasSave() const {
    Preferences prefs;
    if (!prefs.begin(NAMESPACE, true)) return false;
    bool exists = prefs.isKey(KEY_BUG);
    prefs.end();
    return exists;
}

bool SaveManager::saveSettings(float fontScale, uint8_t brightness, float gameSpeed, uint8_t idleTimeout) {
    if (isSaving) {
        Serial.println("[Save] Skip concurrent settings save");
        return false;
    }
    isSaving = true;

    Preferences prefs;
    if (!prefs.begin(NAMESPACE, false)) {
        isSaving = false;
        return false;
    }
    prefs.putFloat(KEY_FONT, fontScale);
    prefs.putUChar(KEY_BRI, brightness);
    prefs.putFloat(KEY_SPEED, gameSpeed);
    prefs.putUChar(KEY_IDLE, idleTimeout);
    prefs.end();

    isSaving = false;
    Serial.printf("[Save] Settings saved: font=%.2f bri=%d speed=%.1f idle=%d\n",
                  fontScale, brightness, gameSpeed, idleTimeout);
    return true;
}

bool SaveManager::loadSettings(float& fontScale, uint8_t& brightness, float& gameSpeed, uint8_t& idleTimeout) {
    Preferences prefs;
    if (!prefs.begin(NAMESPACE, true)) return false;

    bool ok = false;
    if (prefs.isKey(KEY_FONT)) {
        fontScale = prefs.getFloat(KEY_FONT, 1.5f);
        ok = true;
    }
    if (prefs.isKey(KEY_BRI)) {
        brightness = prefs.getUChar(KEY_BRI, 128);
        ok = true;
    }
    if (prefs.isKey(KEY_SPEED)) {
        gameSpeed = prefs.getFloat(KEY_SPEED, 1.0f);
        ok = true;
    }
    if (prefs.isKey(KEY_IDLE)) {
        idleTimeout = prefs.getUChar(KEY_IDLE, 1);
        ok = true;
    }
    prefs.end();
    return ok;
}
