#include "SaveManager.h"
#include <Preferences.h>
#include <Arduino.h>

namespace {
constexpr size_t BUG_SAVE_MAX_BYTES = 192;
constexpr uint8_t TERRARIUM_VIEW_SAVE_VERSION = 3;
constexpr uint8_t PROGRESSION_SAVE_VERSION = 1;

struct ProgressionSaveData {
    uint8_t version;
    uint32_t featureFlags;
    uint16_t foodUnlockMask;
    uint8_t toyBallCount;
    uint8_t activeToyBallDurability;
} __attribute__((packed));

struct TerrariumViewSaveDataV1 {
    uint8_t version;
    uint8_t valid;
    int16_t bugX;
    int16_t bugY;
    uint32_t animFrame;
    uint8_t adultState;
    uint8_t flags;
    uint8_t turnFrameIndex;
    int16_t targetX;
    int16_t slideTargetX;
    int16_t climbTargetX;
    uint32_t stateTimer;
    uint32_t stateDuration;
    uint8_t eatFrameInterval;
    uint8_t eatBitesThisSession;
    uint32_t restResumeAllowedMs;
    uint32_t foodRefillGraceUntilMs;
    uint32_t alertUntilMs;
} __attribute__((packed));

struct TerrariumViewSaveDataV2 {
    uint8_t version;
    uint8_t valid;
    int16_t bugX;
    int16_t bugY;
    uint32_t animFrame;
    uint8_t adultState;
    uint8_t flags;
    uint8_t turnFrameIndex;
    int16_t targetX;
    int16_t slideTargetX;
    int16_t climbTargetX;
    uint32_t stateTimer;
    uint32_t stateDuration;
    uint8_t eatFrameInterval;
    uint8_t eatBitesThisSession;
    uint32_t restResumeAllowedMs;
    uint32_t foodRefillGraceUntilMs;
    uint32_t alertUntilMs;
    uint8_t substrateLevels[TerrariumViewState::SUBSTRATE_SLOT_COUNT];
    uint8_t substrateBites[TerrariumViewState::SUBSTRATE_SLOT_COUNT];
    uint8_t substrateDropActiveMask;
    uint8_t substrateDropSlots[TerrariumViewState::SUBSTRATE_DROP_SLOTS];
    uint8_t substrateDropLevels[TerrariumViewState::SUBSTRATE_DROP_SLOTS];
    int16_t substrateDropY[TerrariumViewState::SUBSTRATE_DROP_SLOTS];
    int16_t substrateDropVy[TerrariumViewState::SUBSTRATE_DROP_SLOTS];
} __attribute__((packed));

struct TerrariumViewSaveData {
    uint8_t version;
    uint8_t valid;
    int16_t bugX;
    int16_t bugY;
    uint8_t bugZ;
    uint32_t animFrame;
    uint8_t adultState;
    uint8_t flags;
    uint8_t turnFrameIndex;
    int16_t targetX;
    uint8_t targetZ;
    int16_t slideTargetX;
    int16_t climbTargetX;
    uint32_t stateTimer;
    uint32_t stateDuration;
    uint8_t eatFrameInterval;
    uint8_t eatBitesThisSession;
    uint32_t restResumeAllowedMs;
    uint32_t foodRefillGraceUntilMs;
    uint32_t alertUntilMs;
    uint8_t substrateLevels[TerrariumViewState::SUBSTRATE_SLOT_COUNT];
    uint8_t substrateBites[TerrariumViewState::SUBSTRATE_SLOT_COUNT];
    uint8_t substrateDropActiveMask;
    uint8_t substrateDropSlots[TerrariumViewState::SUBSTRATE_DROP_SLOTS];
    uint8_t substrateDropLevels[TerrariumViewState::SUBSTRATE_DROP_SLOTS];
    int16_t substrateDropY[TerrariumViewState::SUBSTRATE_DROP_SLOTS];
    int16_t substrateDropVy[TerrariumViewState::SUBSTRATE_DROP_SLOTS];
} __attribute__((packed));

static_assert(sizeof(TerrariumViewSaveDataV1) <= 64, "Terrarium view v1 save data too large");
static_assert(sizeof(TerrariumViewSaveDataV2) <= 128, "Terrarium view v2 save data too large");
static_assert(sizeof(TerrariumViewSaveData) <= 128, "Terrarium view save data too large");

uint8_t packTerrariumFlags(const TerrariumViewState& state) {
    uint8_t flags = 0;
    if (state.faceRight) flags |= 1 << 0;
    if (state.turnTargetFaceRight) flags |= 1 << 1;
    if (state.walkAfterTurn) flags |= 1 << 2;
    if (state.slideAfterTurn) flags |= 1 << 3;
    if (state.climbAfterTurn) flags |= 1 << 4;
    if (state.tiltHighSideIsRight) flags |= 1 << 5;
    return flags;
}

void unpackTerrariumFlags(uint8_t flags, TerrariumViewState& state) {
    state.faceRight = (flags & (1 << 0)) != 0;
    state.turnTargetFaceRight = (flags & (1 << 1)) != 0;
    state.walkAfterTurn = (flags & (1 << 2)) != 0;
    state.slideAfterTurn = (flags & (1 << 3)) != 0;
    state.climbAfterTurn = (flags & (1 << 4)) != 0;
    state.tiltHighSideIsRight = (flags & (1 << 5)) != 0;
}
}

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
    if (ver != SAVE_VERSION && ver != 8 && ver != 7 && ver != 6 && ver != 5) {
        Serial.printf("[Save] Version mismatch: stored=%d expected=%d\n", ver, SAVE_VERSION);
        prefs.end();
        return false;
    }

    size_t len = prefs.getBytesLength(KEY_BUG);
    bool ok = false;
    if (len > 0 && len <= BUG_SAVE_MAX_BYTES) {
        uint8_t buf[BUG_SAVE_MAX_BYTES];
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

    uint8_t buf[BUG_SAVE_MAX_BYTES];
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

bool SaveManager::saveCupGlobal(uint16_t season, uint32_t lastCupGameTime, uint8_t state) {
    Preferences prefs;
    if (!prefs.begin(NAMESPACE, false)) return false;
    prefs.putUShort(KEY_CUP_SEASON, season);
    prefs.putUInt(KEY_CUP_TIME, lastCupGameTime);        // 旧 key，保持兼容
    prefs.putUInt(KEY_CUP_GAME_TIME, lastCupGameTime);   // 新 key，语义为游戏时间秒
    prefs.putUChar(KEY_CUP_STATE, state);
    prefs.end();
    return true;
}

bool SaveManager::loadCupGlobal(uint16_t& season, uint32_t& lastCupGameTime, uint8_t& state) {
    Preferences prefs;
    if (!prefs.begin(NAMESPACE, true)) return false;
    season = prefs.getUShort(KEY_CUP_SEASON, 0);
    // 优先读取新 key；不存在则回退旧 key（首次升级）
    if (prefs.isKey(KEY_CUP_GAME_TIME)) {
        lastCupGameTime = prefs.getUInt(KEY_CUP_GAME_TIME, 0);
    } else {
        lastCupGameTime = prefs.getUInt(KEY_CUP_TIME, 0);
    }
    state = prefs.getUChar(KEY_CUP_STATE, 0);
    prefs.end();
    return true;
}

void SaveManager::clearCupGlobal() {
    Preferences prefs;
    if (prefs.begin(NAMESPACE, false)) {
        prefs.remove(KEY_CUP_SEASON);
        prefs.remove(KEY_CUP_TIME);
        prefs.remove(KEY_CUP_GAME_TIME);
        prefs.remove(KEY_CUP_STATE);
        prefs.end();
    }
}

bool SaveManager::saveExploreGlobal(uint32_t day, uint8_t timeOfDay, uint8_t countToday) {
    Preferences prefs;
    if (!prefs.begin(NAMESPACE, false)) return false;
    prefs.putUInt(KEY_EXPLORE_DAY, day);
    prefs.putUChar(KEY_EXPLORE_TOD, timeOfDay);
    prefs.putUChar(KEY_EXPLORE_COUNT, countToday);
    prefs.end();
    return true;
}

bool SaveManager::loadExploreGlobal(uint32_t& day, uint8_t& timeOfDay, uint8_t& countToday) {
    Preferences prefs;
    if (!prefs.begin(NAMESPACE, true)) return false;
    bool ok = prefs.isKey(KEY_EXPLORE_DAY) ||
              prefs.isKey(KEY_EXPLORE_TOD) ||
              prefs.isKey(KEY_EXPLORE_COUNT);
    day = prefs.getUInt(KEY_EXPLORE_DAY, 0);
    timeOfDay = prefs.getUChar(KEY_EXPLORE_TOD, 0);
    countToday = prefs.getUChar(KEY_EXPLORE_COUNT, 0);
    prefs.end();
    return ok;
}

void SaveManager::clearExploreGlobal() {
    Preferences prefs;
    if (prefs.begin(NAMESPACE, false)) {
        prefs.remove(KEY_EXPLORE_DAY);
        prefs.remove(KEY_EXPLORE_TOD);
        prefs.remove(KEY_EXPLORE_COUNT);
        prefs.end();
    }
}

bool SaveManager::saveProgression(const ProgressionState& state) {
    Preferences prefs;
    if (!prefs.begin(NAMESPACE, false)) return false;

    ProgressionSaveData sd = {};
    sd.version = PROGRESSION_SAVE_VERSION;
    sd.featureFlags = state.featureFlags;
    sd.foodUnlockMask = state.foodUnlockMask;
    sd.toyBallCount = state.toyBallCount;
    sd.activeToyBallDurability = state.activeToyBallDurability;
    prefs.putBytes(KEY_PROGRESSION, &sd, sizeof(sd));
    prefs.end();
    Serial.printf("[Save] Progression saved: flags=%lu foodMask=0x%04X ball=%u dur=%u\n",
                  (unsigned long)state.featureFlags,
                  state.foodUnlockMask,
                  state.toyBallCount,
                  state.activeToyBallDurability);
    return true;
}

bool SaveManager::loadProgression(ProgressionState& state) {
    Preferences prefs;
    if (!prefs.begin(NAMESPACE, true)) return false;

    size_t len = prefs.getBytesLength(KEY_PROGRESSION);
    if (len != sizeof(ProgressionSaveData)) {
        prefs.end();
        state = ProgressionState();
        return false;
    }

    ProgressionSaveData sd = {};
    prefs.getBytes(KEY_PROGRESSION, &sd, sizeof(sd));
    prefs.end();
    if (sd.version != PROGRESSION_SAVE_VERSION) {
        state = ProgressionState();
        return false;
    }

    state.featureFlags = sd.featureFlags;
    state.foodUnlockMask = sd.foodUnlockMask;
    state.toyBallCount = sd.toyBallCount;
    state.activeToyBallDurability = sd.activeToyBallDurability;
    state.foodUnlockMask |= (uint16_t)(1U << (uint8_t)FoodType::DROP);
    Serial.printf("[Save] Progression loaded: flags=%lu foodMask=0x%04X ball=%u dur=%u\n",
                  (unsigned long)state.featureFlags,
                  state.foodUnlockMask,
                  state.toyBallCount,
                  state.activeToyBallDurability);
    return true;
}

void SaveManager::clearProgression() {
    Preferences prefs;
    if (prefs.begin(NAMESPACE, false)) {
        prefs.remove(KEY_PROGRESSION);
        prefs.end();
    }
}

bool SaveManager::saveTerrariumViewState(const TerrariumViewState& state) {
    Preferences prefs;
    if (!prefs.begin(NAMESPACE, false)) return false;

    if (!state.valid) {
        prefs.remove(KEY_TERRARIUM_VIEW);
        prefs.end();
        return true;
    }

    TerrariumViewSaveData sd = {};
    sd.version = TERRARIUM_VIEW_SAVE_VERSION;
    sd.valid = state.valid ? 1 : 0;
    sd.bugX = (int16_t)state.bugX;
    sd.bugY = (int16_t)state.bugY;
    sd.bugZ = state.bugZ;
    sd.animFrame = state.animFrame;
    sd.adultState = state.adultState;
    sd.flags = packTerrariumFlags(state);
    sd.turnFrameIndex = state.turnFrameIndex;
    sd.targetX = (int16_t)state.targetX;
    sd.targetZ = state.targetZ;
    sd.slideTargetX = (int16_t)state.slideTargetX;
    sd.climbTargetX = (int16_t)state.climbTargetX;
    sd.stateTimer = state.stateTimer;
    sd.stateDuration = state.stateDuration;
    sd.eatFrameInterval = state.eatFrameInterval;
    sd.eatBitesThisSession = state.eatBitesThisSession;
    sd.restResumeAllowedMs = state.restResumeAllowedMs;
    sd.foodRefillGraceUntilMs = state.foodRefillGraceUntilMs;
    sd.alertUntilMs = state.alertUntilMs;
    for (uint8_t i = 0; i < TerrariumViewState::SUBSTRATE_SLOT_COUNT; ++i) {
        sd.substrateLevels[i] = state.substrateLevels[i];
        sd.substrateBites[i] = state.substrateBites[i];
    }
    sd.substrateDropActiveMask = state.substrateDropActiveMask;
    for (uint8_t i = 0; i < TerrariumViewState::SUBSTRATE_DROP_SLOTS; ++i) {
        sd.substrateDropSlots[i] = state.substrateDropSlots[i];
        sd.substrateDropLevels[i] = state.substrateDropLevels[i];
        sd.substrateDropY[i] = state.substrateDropY[i];
        sd.substrateDropVy[i] = state.substrateDropVy[i];
    }

    prefs.putBytes(KEY_TERRARIUM_VIEW, &sd, sizeof(sd));
    prefs.end();
    Serial.printf("[Save] Terrarium view saved: state=%u x=%d z=%d faceRight=%d\n",
                  state.adultState, state.bugX, state.bugZ, state.faceRight ? 1 : 0);
    return true;
}

bool SaveManager::loadTerrariumViewState(TerrariumViewState& state) {
    Preferences prefs;
    if (!prefs.begin(NAMESPACE, true)) return false;

    size_t len = prefs.getBytesLength(KEY_TERRARIUM_VIEW);
    if (len != sizeof(TerrariumViewSaveData) &&
        len != sizeof(TerrariumViewSaveDataV2) &&
        len != sizeof(TerrariumViewSaveDataV1)) {
        prefs.end();
        state = TerrariumViewState();
        return false;
    }

    if (len == sizeof(TerrariumViewSaveDataV1)) {
        TerrariumViewSaveDataV1 sd = {};
        prefs.getBytes(KEY_TERRARIUM_VIEW, &sd, sizeof(sd));
        prefs.end();

        if (sd.version != 1 || sd.valid == 0) {
            state = TerrariumViewState();
            return false;
        }

        state = TerrariumViewState();
        state.valid = true;
        state.bugX = sd.bugX;
        state.bugY = sd.bugY;
        state.bugZ = 0;
        state.animFrame = sd.animFrame;
        state.adultState = sd.adultState;
        unpackTerrariumFlags(sd.flags, state);
        state.turnFrameIndex = sd.turnFrameIndex;
        state.targetX = sd.targetX;
        state.targetZ = 0;
        state.slideTargetX = sd.slideTargetX;
        state.climbTargetX = sd.climbTargetX;
        state.stateTimer = sd.stateTimer;
        state.stateDuration = sd.stateDuration;
        state.eatFrameInterval = sd.eatFrameInterval;
        state.eatBitesThisSession = sd.eatBitesThisSession;
        state.restResumeAllowedMs = sd.restResumeAllowedMs;
        state.foodRefillGraceUntilMs = sd.foodRefillGraceUntilMs;
        state.alertUntilMs = sd.alertUntilMs;

        Serial.printf("[Save] Terrarium view loaded: state=%u x=%d faceRight=%d\n",
                      state.adultState, state.bugX, state.faceRight ? 1 : 0);
        return true;
    }

    if (len == sizeof(TerrariumViewSaveDataV2)) {
        TerrariumViewSaveDataV2 sd = {};
        prefs.getBytes(KEY_TERRARIUM_VIEW, &sd, sizeof(sd));
        prefs.end();

        if (sd.version != 2 || sd.valid == 0) {
            state = TerrariumViewState();
            return false;
        }

        state = TerrariumViewState();
        state.valid = true;
        state.bugX = sd.bugX;
        state.bugY = sd.bugY;
        state.bugZ = 0;
        state.animFrame = sd.animFrame;
        state.adultState = sd.adultState;
        unpackTerrariumFlags(sd.flags, state);
        state.turnFrameIndex = sd.turnFrameIndex;
        state.targetX = sd.targetX;
        state.targetZ = 0;
        state.slideTargetX = sd.slideTargetX;
        state.climbTargetX = sd.climbTargetX;
        state.stateTimer = sd.stateTimer;
        state.stateDuration = sd.stateDuration;
        state.eatFrameInterval = sd.eatFrameInterval;
        state.eatBitesThisSession = sd.eatBitesThisSession;
        state.restResumeAllowedMs = sd.restResumeAllowedMs;
        state.foodRefillGraceUntilMs = sd.foodRefillGraceUntilMs;
        state.alertUntilMs = sd.alertUntilMs;
        for (uint8_t i = 0; i < TerrariumViewState::SUBSTRATE_SLOT_COUNT; ++i) {
            state.substrateLevels[i] = sd.substrateLevels[i];
            state.substrateBites[i] = sd.substrateBites[i];
        }
        state.substrateDropActiveMask = sd.substrateDropActiveMask;
        for (uint8_t i = 0; i < TerrariumViewState::SUBSTRATE_DROP_SLOTS; ++i) {
            state.substrateDropSlots[i] = sd.substrateDropSlots[i];
            state.substrateDropLevels[i] = sd.substrateDropLevels[i];
            state.substrateDropY[i] = sd.substrateDropY[i];
            state.substrateDropVy[i] = sd.substrateDropVy[i];
        }

        Serial.printf("[Save] Terrarium view loaded: state=%u x=%d z=%d faceRight=%d\n",
                      state.adultState, state.bugX, state.bugZ, state.faceRight ? 1 : 0);
        return true;
    }

    TerrariumViewSaveData sd = {};
    prefs.getBytes(KEY_TERRARIUM_VIEW, &sd, sizeof(sd));
    prefs.end();

    if (sd.version != TERRARIUM_VIEW_SAVE_VERSION || sd.valid == 0) {
        state = TerrariumViewState();
        return false;
    }

    state = TerrariumViewState();
    state.valid = true;
    state.bugX = sd.bugX;
    state.bugY = sd.bugY;
    state.bugZ = sd.bugZ;
    state.animFrame = sd.animFrame;
    state.adultState = sd.adultState;
    unpackTerrariumFlags(sd.flags, state);
    state.turnFrameIndex = sd.turnFrameIndex;
    state.targetX = sd.targetX;
    state.targetZ = sd.targetZ;
    state.slideTargetX = sd.slideTargetX;
    state.climbTargetX = sd.climbTargetX;
    state.stateTimer = sd.stateTimer;
    state.stateDuration = sd.stateDuration;
    state.eatFrameInterval = sd.eatFrameInterval;
    state.eatBitesThisSession = sd.eatBitesThisSession;
    state.restResumeAllowedMs = sd.restResumeAllowedMs;
    state.foodRefillGraceUntilMs = sd.foodRefillGraceUntilMs;
    state.alertUntilMs = sd.alertUntilMs;
    for (uint8_t i = 0; i < TerrariumViewState::SUBSTRATE_SLOT_COUNT; ++i) {
        state.substrateLevels[i] = sd.substrateLevels[i];
        state.substrateBites[i] = sd.substrateBites[i];
    }
    state.substrateDropActiveMask = sd.substrateDropActiveMask;
    for (uint8_t i = 0; i < TerrariumViewState::SUBSTRATE_DROP_SLOTS; ++i) {
        state.substrateDropSlots[i] = sd.substrateDropSlots[i];
        state.substrateDropLevels[i] = sd.substrateDropLevels[i];
        state.substrateDropY[i] = sd.substrateDropY[i];
        state.substrateDropVy[i] = sd.substrateDropVy[i];
    }

    Serial.printf("[Save] Terrarium view loaded: state=%u x=%d z=%d faceRight=%d\n",
                  state.adultState, state.bugX, state.bugZ, state.faceRight ? 1 : 0);
    return true;
}

void SaveManager::clearTerrariumViewState() {
    Preferences prefs;
    if (prefs.begin(NAMESPACE, false)) {
        prefs.remove(KEY_TERRARIUM_VIEW);
        prefs.end();
    }
}

bool SaveManager::hasSave() const {
    Preferences prefs;
    if (!prefs.begin(NAMESPACE, true)) return false;
    bool exists = prefs.isKey(KEY_BUG);
    prefs.end();
    return exists;
}

bool SaveManager::saveSettings(float fontScale, uint8_t brightness, float gameSpeed,
                               uint8_t idleTimeout, uint8_t mainSceneBg,
                               uint8_t woodStyle, uint8_t bowlStyle, uint8_t foodStyle,
                               uint8_t toyStyle) {
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
    prefs.putUChar(KEY_BG, mainSceneBg);
    prefs.putUChar(KEY_WOOD, woodStyle);
    prefs.putUChar(KEY_BOWL, bowlStyle);
    prefs.putUChar(KEY_FOOD, foodStyle);
    prefs.putUChar(KEY_TOY, toyStyle);
    prefs.end();

    isSaving = false;
    Serial.printf("[Save] Settings saved: font=%.2f bri=%d speed=%.1f idle=%d bg=%d wood=%d bowl=%d food=%d toy=%d\n",
                  fontScale, brightness, gameSpeed, idleTimeout,
                  mainSceneBg, woodStyle, bowlStyle, foodStyle, toyStyle);
    return true;
}

bool SaveManager::loadSettings(float& fontScale, uint8_t& brightness, float& gameSpeed,
                               uint8_t& idleTimeout, uint8_t& mainSceneBg,
                               uint8_t& woodStyle, uint8_t& bowlStyle, uint8_t& foodStyle,
                               uint8_t& toyStyle) {
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
    if (prefs.isKey(KEY_BG)) {
        mainSceneBg = prefs.getUChar(KEY_BG, 0);
        ok = true;
    }
    if (prefs.isKey(KEY_WOOD)) {
        woodStyle = prefs.getUChar(KEY_WOOD, 0);
        ok = true;
    }
    if (prefs.isKey(KEY_BOWL)) {
        bowlStyle = prefs.getUChar(KEY_BOWL, 0);
        ok = true;
    }
    if (prefs.isKey(KEY_FOOD)) {
        foodStyle = prefs.getUChar(KEY_FOOD, 0);
        ok = true;
    }
    if (prefs.isKey(KEY_TOY)) {
        toyStyle = prefs.getUChar(KEY_TOY, 0);
        ok = true;
    }
    prefs.end();
    return ok;
}
