#pragma once

#include <cstdint>

// 培养缸主界面的运行时表现状态。
// 场景切换与设备重启后用于恢复成虫位置、朝向和当前动作。
struct TerrariumViewState {
    static constexpr uint8_t SUBSTRATE_SLOT_COUNT = 15;
    static constexpr uint8_t SUBSTRATE_DROP_SLOTS = 5;

    bool valid = false;
    int bugX = 120;
    int bugY = 80;
    uint32_t animFrame = 0;
    uint8_t adultState = 0;
    bool faceRight = true;
    bool turnTargetFaceRight = true;
    bool walkAfterTurn = false;
    bool slideAfterTurn = false;
    bool climbAfterTurn = false;
    uint8_t turnFrameIndex = 0;
    int targetX = 120;
    int slideTargetX = 120;
    int climbTargetX = 120;
    bool tiltHighSideIsRight = true;
    uint32_t stateTimer = 0;
    uint32_t stateDuration = 0;
    uint8_t eatFrameInterval = 0;
    uint8_t eatBitesThisSession = 0;
    uint32_t restResumeAllowedMs = 0;
    uint32_t foodRefillGraceUntilMs = 0;
    uint32_t alertUntilMs = 0;

    bool visitorActive = false;
    bool visitorFalling = false;
    int visitorX = 168;
    int visitorY = 125;
    int visitorFromY = -30;
    int visitorTargetY = 125;
    uint8_t visitorSiz = 8;
    uint8_t visitorPalette = 0x80;
    bool visitorFaceRight = false;
    uint32_t visitorRemainingMs = 0;
    uint32_t visitorDropElapsedMs = 0;

    uint8_t substrateLevels[SUBSTRATE_SLOT_COUNT] = {};
    uint8_t substrateBites[SUBSTRATE_SLOT_COUNT] = {};
    uint8_t substrateDropActiveMask = 0;
    uint8_t substrateDropSlots[SUBSTRATE_DROP_SLOTS] = {};
    uint8_t substrateDropLevels[SUBSTRATE_DROP_SLOTS] = {};
    int16_t substrateDropY[SUBSTRATE_DROP_SLOTS] = {};
    int16_t substrateDropVy[SUBSTRATE_DROP_SLOTS] = {};
};
