#include "BattleLink.h"
#include "Hal.h"
#include <esp_wifi.h>
#include <cstring>

const uint8_t BattleLink::BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

namespace {

constexpr bool BATTLE_LINK_PACKET_LOGS = false;
constexpr uint32_t BATTLE_LINK_WARN_LOG_INTERVAL_MS = 10000;

bool isVisitTrackedMessage(uint8_t type) {
    return type == MSG_VISIT_RECALL ||
           type == MSG_VISIT_PING ||
           type == MSG_VISIT_STATUS ||
           type == MSG_VISIT_INTENT ||
           type == MSG_VISIT_EAT_RESULT;
}

}

#define BATTLE_LINK_LOG_EVERY_MS(intervalMs, statement) do { \
    static uint32_t lastLogMs = 0; \
    static bool logSeen = false; \
    uint32_t nowLogMs = Hal::ins().millis(); \
    if (!logSeen || nowLogMs - lastLogMs >= (intervalMs)) { \
        logSeen = true; \
        lastLogMs = nowLogMs; \
        statement; \
    } \
} while (false)


#include "battle_link/BattleLinkSessionSend.inc"
#include "battle_link/BattleLinkReceive.inc"
#include "battle_link/BattleLinkHandlersUpdate.inc"
