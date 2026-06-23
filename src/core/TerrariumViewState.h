#pragma once

#include <cstdint>

// 培养缸主界面的运行时表现状态。
// 场景切换与设备重启后用于恢复成虫位置、朝向和当前动作。
struct TerrariumViewState {
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
};
