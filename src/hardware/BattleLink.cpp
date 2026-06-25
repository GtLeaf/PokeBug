#include "BattleLink.h"
#include "Hal.h"
#include <esp_wifi.h>
#include <cstring>

const uint8_t BattleLink::BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

namespace {

constexpr bool BATTLE_LINK_PACKET_LOGS = false;
constexpr uint32_t BATTLE_LINK_WARN_LOG_INTERVAL_MS = 10000;

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

BattleLink& BattleLink::ins() {
    static BattleLink instance;
    return instance;
}

bool BattleLink::begin() {
    if (initialized) return true;

    Serial.println("[BattleLink] begin()");
    if (WiFi.getMode() == WIFI_OFF) {
        WiFi.mode(WIFI_STA);
        Serial.println("[BattleLink] WiFi STA enabled");
    }
    esp_err_t err = esp_wifi_set_ps(WIFI_PS_NONE);
    if (err != ESP_OK) {
        Serial.printf("[BattleLink] esp_wifi_set_ps failed, err=%d\n", err);
    }

    err = esp_now_init();
    if (err != ESP_OK) {
        Serial.printf("[BattleLink] esp_now_init failed, err=%d\n", err);
        return false;
    }
    err = esp_now_register_recv_cb(onRecvStatic);
    if (err != ESP_OK) {
        esp_now_deinit();
        return false;
    }
    err = esp_now_register_send_cb(onSentStatic);
    if (err != ESP_OK) {
        esp_now_unregister_recv_cb();
        esp_now_deinit();
        return false;
    }

    initialized = true;
    sendBusy = false;
    currentSendTracked = false;
    sendState = SendState::IDLE;
    pendingGift = false;
    lastGiftSeen = false;
    pendingVisitVitals = false;
    Serial.println("[BattleLink] initialized");
    return true;
}

void BattleLink::end() {
    if (!initialized) return;
    Serial.println("[BattleLink] end()");
    stopRoom();
    esp_now_unregister_recv_cb();
    esp_now_unregister_send_cb();
    esp_now_del_peer(BROADCAST_MAC);
    esp_now_deinit();
    WiFi.mode(WIFI_OFF);
    initialized = false;
    sendBusy = false;
    currentSendTracked = false;
    sendState = SendState::IDLE;
    battlePeerSet = false;
    pendingGift = false;
    lastGiftSeen = false;
    pendingVisitVitals = false;
}

bool BattleLink::ensureBroadcastPeer() {
    if (!initialized) return false;
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, BROADCAST_MAC, 6);
    peer.channel = 0;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    esp_err_t err = esp_now_add_peer(&peer);
    if (err != ESP_OK && err != ESP_ERR_ESPNOW_EXIST) {
        Serial.printf("[BattleLink] add broadcast peer failed, err=%d\n", err);
        return false;
    }
    return true;
}

bool BattleLink::ensureUnicastPeer(const uint8_t mac[6]) {
    if (!initialized) return false;
    esp_now_del_peer(mac);
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = 0;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    esp_err_t err = esp_now_add_peer(&peer);
    if (err != ESP_OK) {
        Serial.printf("[BattleLink] add unicast peer failed, err=%d\n", err);
        return false;
    }
    return true;
}

// ============================================================
// 房间阶段
// ============================================================
void BattleLink::startRoomHost(uint8_t roomId, uint8_t purpose) {
    if (!initialized) begin();
    roomState = RoomState::HOSTING;
    hostedRoomId = roomId;
    roomPurpose = purpose;
    lastRoomAdvertMs = 0;
    pendingJoinReq = false;
    pendingGift = false;
    lastGiftSeen = false;
    pendingVisitVitals = false;
    uint8_t mac[6];
    getMyMac(mac);
    Serial.printf("[BattleLink] start hosting room=%u purpose=%u my=%02X:%02X:%02X:%02X:%02X:%02X\n",
                  roomId, purpose, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void BattleLink::startRoomSearch(uint8_t purpose) {
    if (!initialized) begin();
    roomState = RoomState::SEARCHING;
    roomPurpose = purpose;
    memset(roomList, 0, sizeof(roomList));
    pendingJoinAck = false;
    pendingGift = false;
    lastGiftSeen = false;
    pendingVisitVitals = false;
    uint8_t mac[6];
    getMyMac(mac);
    Serial.printf("[BattleLink] start searching rooms purpose=%u my=%02X:%02X:%02X:%02X:%02X:%02X\n",
                  purpose, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void BattleLink::stopRoom() {
    Serial.printf("[BattleLink] stop room state=%d hosted=%u purpose=%u\n",
                  (int)roomState, hostedRoomId, roomPurpose);
    roomState = RoomState::IDLE;
    hostedRoomId = 0;
}

bool BattleLink::takeJoinReq(join_req_t& out) {
    if (!pendingJoinReq) return false;
    out = pendingJoinReqData;
    pendingJoinReq = false;
    return true;
}

bool BattleLink::sendJoinAck(bool accept, const uint8_t* clientMac) {
    if (!initialized || !battlePeerSet) {
        Serial.printf("[BattleLink] sendJoinAck: not ready init=%d peer=%d accept=%d client=%02X:%02X:%02X:%02X:%02X:%02X\n",
                      initialized ? 1 : 0, battlePeerSet ? 1 : 0, accept ? 1 : 0,
                      clientMac ? clientMac[0] : 0, clientMac ? clientMac[1] : 0,
                      clientMac ? clientMac[2] : 0, clientMac ? clientMac[3] : 0,
                      clientMac ? clientMac[4] : 0, clientMac ? clientMac[5] : 0);
        return false;
    }
    join_ack_t ack;
    ack.type = MSG_JOIN_ACK;
    getMyMac(ack.host_mac);
    memcpy(ack.client_mac, clientMac, 6);
    ack.accepted = accept ? 1 : 0;
    Serial.printf("[BattleLink] sendJoinAck accept=%d host=%02X:%02X:%02X:%02X:%02X:%02X client=%02X:%02X:%02X:%02X:%02X:%02X\n",
                  accept ? 1 : 0,
                  ack.host_mac[0], ack.host_mac[1], ack.host_mac[2],
                  ack.host_mac[3], ack.host_mac[4], ack.host_mac[5],
                  clientMac[0], clientMac[1], clientMac[2],
                  clientMac[3], clientMac[4], clientMac[5]);
    // 大厅消息不占用对战 sendState，避免阻塞后续 sync
    return sendLobbyPacket(clientMac, (uint8_t*)&ack, sizeof(ack));
}

uint8_t BattleLink::getRoomListCount() const {
    uint32_t now = Hal::ins().millis();
    uint8_t count = 0;
    for (uint8_t i = 0; i < MAX_ROOM_LIST; i++) {
        if (roomList[i].lastSeenMs != 0 && now - roomList[i].lastSeenMs < ROOM_ENTRY_TIMEOUT_MS) {
            count++;
        }
    }
    return count;
}

const BattleLink::RoomEntry* BattleLink::getRoomListEntry(uint8_t idx) const {
    uint32_t now = Hal::ins().millis();
    uint8_t count = 0;
    for (uint8_t i = 0; i < MAX_ROOM_LIST; i++) {
        if (roomList[i].lastSeenMs != 0 && now - roomList[i].lastSeenMs < ROOM_ENTRY_TIMEOUT_MS) {
            if (count == idx) return &roomList[i];
            count++;
        }
    }
    return nullptr;
}

bool BattleLink::sendJoinReq(uint8_t roomId, const uint8_t* hostMac) {
    if (!initialized) return false;
    join_req_t req;
    req.type = MSG_JOIN_REQ;
    getMyMac(req.mac);
    req.room_id = roomId;
    req.purpose = roomPurpose;
    Serial.printf("[BattleLink] sendJoinReq room=%u purpose=%u host=%02X:%02X:%02X:%02X:%02X:%02X from=%02X:%02X:%02X:%02X:%02X:%02X\n",
                  roomId, req.purpose,
                  hostMac[0], hostMac[1], hostMac[2], hostMac[3], hostMac[4], hostMac[5],
                  req.mac[0], req.mac[1], req.mac[2], req.mac[3], req.mac[4], req.mac[5]);
    // 大厅消息不占用对战 sendState
    return sendLobbyPacket(hostMac, (uint8_t*)&req, sizeof(req));
}

bool BattleLink::takeJoinAck(join_ack_t& out) {
    if (!pendingJoinAck) return false;
    out = pendingJoinAckData;
    pendingJoinAck = false;
    return true;
}

void BattleLink::setBattlePeer(const uint8_t* mac, bool asHost) {
    memcpy(battlePeerMac, mac, 6);
    battleAsHost = asHost;
    battlePeerSet = true;
    // 大厅阶段的 join req/ack 可能还在等 ACK，进入对战后不再需要，直接清空发送状态
    sendState = SendState::IDLE;
    pendingVisitVitals = false;
    Serial.printf("[BattleLink] battle peer set host=%d mac=%02X:%02X\n",
                  asHost ? 1 : 0, mac[0], mac[5]);
}

// ============================================================
// 对战发送 API
// ============================================================
bool BattleLink::sendSync(const battle_sync_t& sync) {
    if (!battlePeerSet) { Serial.println("[BattleLink] sendSync: no peer"); return false; }
    if (sendState == SendState::SENDING) { Serial.println("[BattleLink] sendSync: busy"); return false; }
    battle_sync_t packet = sync;
    packet.type = MSG_BATTLE_SYNC;
    packet.version = BATTLE_PROTOCOL_VERSION;
    memcpy(sendCtx.buf, &packet, sizeof(packet));
    sendCtx.len = sizeof(sync);
    memcpy(sendCtx.targetMac, battlePeerMac, 6);
    sendCtx.retryCount = 0;
    sendCtx.ackReceived = false;
    sendCtx.startMs = Hal::ins().millis();
    Serial.println("[BattleLink] sendSync");
    if (!sendInternal(battlePeerMac, sendCtx.buf, sendCtx.len)) return false;
    sendState = SendState::SENDING;
    return true;
}

bool BattleLink::sendReady(const battle_ready_t& ready) {
    if (!battlePeerSet) { Serial.println("[BattleLink] sendReady: no peer"); return false; }
    if (sendState == SendState::SENDING) { Serial.println("[BattleLink] sendReady: busy"); return false; }
    battle_ready_t packet = ready;
    packet.type = MSG_BATTLE_READY;
    packet.version = BATTLE_PROTOCOL_VERSION;
    memcpy(sendCtx.buf, &packet, sizeof(packet));
    sendCtx.len = sizeof(ready);
    memcpy(sendCtx.targetMac, battlePeerMac, 6);
    sendCtx.retryCount = 0;
    sendCtx.ackReceived = false;
    sendCtx.startMs = Hal::ins().millis();
    Serial.printf("[BattleLink] sendReady rhythm round=%d mot=%d\n", ready.round_num, ready.my_mot);
    if (!sendInternal(battlePeerMac, sendCtx.buf, sendCtx.len)) return false;
    sendState = SendState::SENDING;
    return true;
}

bool BattleLink::sendRound(const battle_round_t& round) {
    if (!battlePeerSet) { Serial.println("[BattleLink] sendRound: no peer"); return false; }
    if (sendState == SendState::SENDING) { Serial.println("[BattleLink] sendRound: busy"); return false; }
    battle_round_t packet = round;
    packet.type = MSG_BATTLE_ROUND;
    packet.version = BATTLE_PROTOCOL_VERSION;
    memcpy(sendCtx.buf, &packet, sizeof(packet));
    sendCtx.len = sizeof(round);
    memcpy(sendCtx.targetMac, battlePeerMac, 6);
    sendCtx.retryCount = 0;
    sendCtx.ackReceived = false;
    sendCtx.startMs = Hal::ins().millis();
    Serial.printf("[BattleLink] sendRound hDmg=%d cDmg=%d hHp=%d cHp=%d hGauge=%d cGauge=%d\n",
                  round.host_dmg, round.client_dmg, round.host_hp, round.client_hp,
                  round.host_gauge, round.client_gauge);
    if (!sendInternal(battlePeerMac, sendCtx.buf, sendCtx.len)) return false;
    sendState = SendState::SENDING;
    return true;
}

bool BattleLink::sendResult(bool win) {
    if (!battlePeerSet) { Serial.println("[BattleLink] sendResult: no peer"); return false; }
    if (sendState == SendState::SENDING) { Serial.println("[BattleLink] sendResult: busy"); return false; }
    battle_result_t res = { MSG_BATTLE_RESULT, BATTLE_PROTOCOL_VERSION, win ? (uint8_t)1 : (uint8_t)0 };
    memcpy(sendCtx.buf, &res, sizeof(res));
    sendCtx.len = sizeof(res);
    memcpy(sendCtx.targetMac, battlePeerMac, 6);
    sendCtx.retryCount = 0;
    sendCtx.ackReceived = false;
    sendCtx.startMs = Hal::ins().millis();
    Serial.printf("[BattleLink] sendResult win=%d\n", win ? 1 : 0);
    if (!sendInternal(battlePeerMac, sendCtx.buf, sendCtx.len)) return false;
    sendState = SendState::SENDING;
    return true;
}

bool BattleLink::sendGiftItem(uint16_t itemId, uint8_t amount) {
    if (!battlePeerSet) { Serial.println("[BattleLink] sendGiftItem: no peer"); return false; }
    if (amount == 0) { Serial.println("[BattleLink] sendGiftItem: empty"); return false; }
    if (sendState == SendState::SENDING) { Serial.println("[BattleLink] sendGiftItem: busy"); return false; }

    gift_item_t gift = {
        MSG_GIFT_ITEM,
        BATTLE_PROTOCOL_VERSION,
        (uint16_t)random(1, 65536),
        itemId,
        amount,
    };
    memcpy(sendCtx.buf, &gift, sizeof(gift));
    sendCtx.len = sizeof(gift);
    memcpy(sendCtx.targetMac, battlePeerMac, 6);
    sendCtx.retryCount = 0;
    sendCtx.ackReceived = false;
    sendCtx.startMs = Hal::ins().millis();
    Serial.printf("[BattleLink] sendGiftItem tx=%u id=%u amount=%u\n",
                  gift.transfer_id, itemId, amount);
    if (!sendInternal(battlePeerMac, sendCtx.buf, sendCtx.len)) return false;
    sendState = SendState::SENDING;
    return true;
}

bool BattleLink::sendVisitRecall() {
    if (!battlePeerSet) {
        BATTLE_LINK_LOG_EVERY_MS(BATTLE_LINK_WARN_LOG_INTERVAL_MS,
                                 Serial.println("[BattleLink] visit recall: no peer"));
        return false;
    }
    if (sendState == SendState::SENDING) {
        BATTLE_LINK_LOG_EVERY_MS(BATTLE_LINK_WARN_LOG_INTERVAL_MS,
                                 Serial.println("[BattleLink] visit recall: busy"));
        return false;
    }
    visit_recall_t recall = { MSG_VISIT_RECALL, BATTLE_PROTOCOL_VERSION };
    memcpy(sendCtx.buf, &recall, sizeof(recall));
    sendCtx.len = sizeof(recall);
    memcpy(sendCtx.targetMac, battlePeerMac, 6);
    sendCtx.retryCount = 0;
    sendCtx.ackReceived = false;
    sendCtx.startMs = Hal::ins().millis();
    Serial.println("[BattleLink] sendVisitRecall");
    if (!sendInternal(battlePeerMac, sendCtx.buf, sendCtx.len)) return false;
    sendState = SendState::SENDING;
    return true;
}

bool BattleLink::sendVisitPing(uint8_t hunger, uint8_t motivation) {
    if (!battlePeerSet) {
        BATTLE_LINK_LOG_EVERY_MS(BATTLE_LINK_WARN_LOG_INTERVAL_MS,
                                 Serial.println("[BattleLink] visit ping: no peer"));
        return false;
    }
    if (sendState == SendState::SENDING) {
        BATTLE_LINK_LOG_EVERY_MS(BATTLE_LINK_WARN_LOG_INTERVAL_MS,
                                 Serial.println("[BattleLink] visit ping: busy"));
        return false;
    }
    if (hunger > 100) hunger = 100;
    if (motivation > 100) motivation = 100;
    visit_ping_t ping = { MSG_VISIT_PING, BATTLE_PROTOCOL_VERSION, hunger, motivation };
    memcpy(sendCtx.buf, &ping, sizeof(ping));
    sendCtx.len = sizeof(ping);
    memcpy(sendCtx.targetMac, battlePeerMac, 6);
    sendCtx.retryCount = 0;
    sendCtx.ackReceived = false;
    sendCtx.startMs = Hal::ins().millis();
    if (BATTLE_LINK_PACKET_LOGS) Serial.println("[BattleLink] sendVisitPing");
    if (!sendInternal(battlePeerMac, sendCtx.buf, sendCtx.len)) return false;
    sendState = SendState::SENDING;
    return true;
}

bool BattleLink::sendVisitStatus(uint32_t remainingMs, uint32_t durationMs, uint8_t speedX10, bool active) {
    if (!battlePeerSet) {
        BATTLE_LINK_LOG_EVERY_MS(BATTLE_LINK_WARN_LOG_INTERVAL_MS,
                                 Serial.println("[BattleLink] visit status: no peer"));
        return false;
    }
    if (sendState == SendState::SENDING) {
        BATTLE_LINK_LOG_EVERY_MS(BATTLE_LINK_WARN_LOG_INTERVAL_MS,
                                 Serial.println("[BattleLink] visit status: busy"));
        return false;
    }
    if (remainingMs > 0xFFFFUL * 1000UL) remainingMs = 0xFFFFUL * 1000UL;
    if (durationMs > 0xFFFFUL * 1000UL) durationMs = 0xFFFFUL * 1000UL;
    if (speedX10 == 0) speedX10 = 10;
    visit_status_t status = {
        MSG_VISIT_STATUS,
        BATTLE_PROTOCOL_VERSION,
        active ? (uint8_t)1 : (uint8_t)0,
        (uint16_t)((remainingMs + 999UL) / 1000UL),
        (uint16_t)((durationMs + 999UL) / 1000UL),
        speedX10,
    };
    memcpy(sendCtx.buf, &status, sizeof(status));
    sendCtx.len = sizeof(status);
    memcpy(sendCtx.targetMac, battlePeerMac, 6);
    sendCtx.retryCount = 0;
    sendCtx.ackReceived = false;
    sendCtx.startMs = Hal::ins().millis();
    if (BATTLE_LINK_PACKET_LOGS) {
        Serial.printf("[BattleLink] sendVisitStatus active=%u remain=%u duration=%u speed=%u\n",
                      status.flags & 1, status.remaining_s, status.duration_s, status.speed_x10);
    }
    if (!sendInternal(battlePeerMac, sendCtx.buf, sendCtx.len)) return false;
    sendState = SendState::SENDING;
    return true;
}

bool BattleLink::sendVisitIntent(uint8_t intent) {
    if (!battlePeerSet) {
        BATTLE_LINK_LOG_EVERY_MS(BATTLE_LINK_WARN_LOG_INTERVAL_MS,
                                 Serial.println("[BattleLink] visit intent: no peer"));
        return false;
    }
    if (sendState == SendState::SENDING) {
        BATTLE_LINK_LOG_EVERY_MS(BATTLE_LINK_WARN_LOG_INTERVAL_MS,
                                 Serial.println("[BattleLink] visit intent: busy"));
        return false;
    }
    visit_intent_t packet = { MSG_VISIT_INTENT, BATTLE_PROTOCOL_VERSION, intent };
    memcpy(sendCtx.buf, &packet, sizeof(packet));
    sendCtx.len = sizeof(packet);
    memcpy(sendCtx.targetMac, battlePeerMac, 6);
    sendCtx.retryCount = 0;
    sendCtx.ackReceived = false;
    sendCtx.startMs = Hal::ins().millis();
    if (BATTLE_LINK_PACKET_LOGS) {
        Serial.printf("[BattleLink] sendVisitIntent intent=%u\n", intent);
    }
    if (!sendInternal(battlePeerMac, sendCtx.buf, sendCtx.len)) return false;
    sendState = SendState::SENDING;
    return true;
}

bool BattleLink::sendVisitEatResult(bool success, uint8_t hungerGain,
                                    uint8_t newGuestHunger, uint8_t foodType) {
    if (!battlePeerSet) {
        BATTLE_LINK_LOG_EVERY_MS(BATTLE_LINK_WARN_LOG_INTERVAL_MS,
                                 Serial.println("[BattleLink] visit eat result: no peer"));
        return false;
    }
    if (sendState == SendState::SENDING) {
        BATTLE_LINK_LOG_EVERY_MS(BATTLE_LINK_WARN_LOG_INTERVAL_MS,
                                 Serial.println("[BattleLink] visit eat result: busy"));
        return false;
    }
    visit_eat_result_t result = {
        MSG_VISIT_EAT_RESULT,
        BATTLE_PROTOCOL_VERSION,
        success ? (uint8_t)1 : (uint8_t)0,
        hungerGain,
        newGuestHunger,
        foodType,
    };
    memcpy(sendCtx.buf, &result, sizeof(result));
    sendCtx.len = sizeof(result);
    memcpy(sendCtx.targetMac, battlePeerMac, 6);
    sendCtx.retryCount = 0;
    sendCtx.ackReceived = false;
    sendCtx.startMs = Hal::ins().millis();
    if (BATTLE_LINK_PACKET_LOGS) {
        Serial.printf("[BattleLink] sendVisitEatResult success=%u gain=%u hun=%u food=%u\n",
                      result.success, result.hunger_gain, result.new_guest_hunger, result.food_type);
    }
    if (!sendInternal(battlePeerMac, sendCtx.buf, sendCtx.len)) return false;
    sendState = SendState::SENDING;
    return true;
}

bool BattleLink::sendInternal(const uint8_t mac[6], const uint8_t* data, uint8_t len) {
    if (!initialized || sendBusy) return false;
    ensureUnicastPeer(mac);
    sendBusy = true;
    currentSendTracked = true;
    esp_err_t err = esp_now_send(mac, data, len);
    if (err != ESP_OK) {
        sendBusy = false;
        currentSendTracked = false;
        BATTLE_LINK_LOG_EVERY_MS(BATTLE_LINK_WARN_LOG_INTERVAL_MS,
                                 Serial.printf("[BattleLink] send failed, err=%d\n", err));
        return false;
    }
    return true;
}

bool BattleLink::sendAckPacket(const uint8_t mac[6], uint8_t ackedType) {
    if (!initialized || sendBusy) return false;
    ensureUnicastPeer(mac);
    ack_msg_t ack = { MSG_ACK, ackedType };
    sendBusy = true;
    currentSendTracked = false;
    esp_err_t err = esp_now_send(mac, (uint8_t*)&ack, sizeof(ack));
    if (err != ESP_OK) {
        sendBusy = false;
        BATTLE_LINK_LOG_EVERY_MS(BATTLE_LINK_WARN_LOG_INTERVAL_MS,
                                 Serial.printf("[BattleLink] ack send failed, err=%d\n", err));
        return false;
    }
    return true;
}

bool BattleLink::sendLobbyPacket(const uint8_t mac[6], const uint8_t* data, uint8_t len) {
    if (!initialized) return false;
    ensureUnicastPeer(mac);
    esp_err_t err = esp_now_send(mac, data, len);
    if (err != ESP_OK) {
        BATTLE_LINK_LOG_EVERY_MS(
            BATTLE_LINK_WARN_LOG_INTERVAL_MS,
            Serial.printf("[BattleLink] lobby send failed type=%u len=%u target=%02X:%02X:%02X:%02X:%02X:%02X err=%d sendBusy=%d\n",
                          len > 0 ? data[0] : 0, len,
                          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                          err, sendBusy ? 1 : 0));
        return false;
    }
    if (BATTLE_LINK_PACKET_LOGS) {
        Serial.printf("[BattleLink] lobby send queued type=%u len=%u target=%02X:%02X:%02X:%02X:%02X:%02X sendBusy=%d\n",
                      len > 0 ? data[0] : 0, len,
                      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                      sendBusy ? 1 : 0);
    }
    return true;
}

bool BattleLink::takeLastSendSuccess() {
    if (sendState == SendState::SENDING) return false;
    bool ok = (sendState == SendState::SUCCESS);
    sendState = SendState::IDLE;
    return ok;
}

// ============================================================
// 接收 API
// ============================================================
bool BattleLink::takeReceivedSync(battle_sync_t& out) {
    if (!pendingSync) return false;
    out = pendingSyncData;
    pendingSync = false;
    return true;
}

bool BattleLink::takeReceivedReady(battle_ready_t& out) {
    if (!pendingReady) return false;
    out = pendingReadyData;
    pendingReady = false;
    return true;
}

bool BattleLink::takeReceivedRound(battle_round_t& out) {
    if (!pendingRound) return false;
    out = pendingRoundData;
    pendingRound = false;
    return true;
}

bool BattleLink::takeReceivedResult(bool& outWin) {
    if (!pendingResult) return false;
    outWin = pendingResultWin;
    pendingResult = false;
    return true;
}

bool BattleLink::takeReceivedGift(gift_item_t& out) {
    if (!pendingGift) return false;
    out = pendingGiftData;
    pendingGift = false;
    return true;
}

bool BattleLink::takeReceivedVisitRecall() {
    if (!pendingVisitRecall) return false;
    pendingVisitRecall = false;
    return true;
}

bool BattleLink::takeReceivedVisitVitals(uint8_t& outHunger, uint8_t& outMotivation) {
    if (!pendingVisitVitals) return false;
    outHunger = pendingVisitHunger;
    outMotivation = pendingVisitMotivation;
    pendingVisitVitals = false;
    return true;
}

bool BattleLink::takeReceivedVisitStatus(visit_status_t& out) {
    if (!pendingVisitStatus) return false;
    out = pendingVisitStatusData;
    pendingVisitStatus = false;
    return true;
}

bool BattleLink::takeReceivedVisitIntent(uint8_t& outIntent) {
    if (!pendingVisitIntent) return false;
    outIntent = pendingVisitIntentData;
    pendingVisitIntent = false;
    return true;
}

bool BattleLink::takeReceivedVisitEatResult(visit_eat_result_t& out) {
    if (!pendingVisitEatResult) return false;
    out = pendingVisitEatResultData;
    pendingVisitEatResult = false;
    return true;
}

// ============================================================
// 回调
// ============================================================
void BattleLink::onRecvStatic(const uint8_t* mac, const uint8_t* data, int len) {
    ins().onRecv(mac, data, len);
}

void BattleLink::onSentStatic(const uint8_t* mac, esp_now_send_status_t status) {
    ins().onSent(mac, status);
}

void BattleLink::onSent(const uint8_t* mac, esp_now_send_status_t status) {
    bool tracked = currentSendTracked;
    sendBusy = false;
    currentSendTracked = false;
    if (BATTLE_LINK_PACKET_LOGS) {
        Serial.printf("[BattleLink] sent to %02X:%02X:%02X:%02X:%02X:%02X status=%s\n",
                      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                      status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
    }

    if (!tracked) return;

    if (status != ESP_NOW_SEND_SUCCESS && sendState == SendState::SENDING) {
        sendCtx.retryCount++;
        if (sendCtx.retryCount > MAX_RETRIES) {
            sendState = SendState::FAIL;
        }
    }
}

void BattleLink::onRecv(const uint8_t* mac, const uint8_t* data, int len) {
    if (len < 1) return;
    uint8_t type = data[0];

    switch (type) {
        case MSG_ROOM_ADVERT:
            if (len == sizeof(room_advert_t)) {
                room_advert_t a;
                memcpy(&a, data, sizeof(a));
                handleRoomAdvert(mac, a);
            }
            break;
        case MSG_JOIN_REQ:
            if (len == sizeof(join_req_t)) {
                join_req_t r;
                memcpy(&r, data, sizeof(r));
                handleJoinReq(mac, r);
            }
            break;
        case MSG_JOIN_ACK:
            if (len == sizeof(join_ack_t)) {
                join_ack_t a;
                memcpy(&a, data, sizeof(a));
                handleJoinAck(mac, a);
            }
            break;
        case MSG_BATTLE_SYNC:
            if (len == sizeof(battle_sync_t)) {
                battle_sync_t s;
                memcpy(&s, data, sizeof(s));
                handleSync(mac, s);
            }
            break;
        case MSG_BATTLE_READY:
            if (len == sizeof(battle_ready_t)) {
                battle_ready_t r;
                memcpy(&r, data, sizeof(r));
                handleReady(mac, r);
            }
            break;
        case MSG_BATTLE_ROUND:
            if (len == sizeof(battle_round_t)) {
                battle_round_t r;
                memcpy(&r, data, sizeof(r));
                handleRound(mac, r);
            }
            break;
        case MSG_BATTLE_RESULT:
            if (len == sizeof(battle_result_t)) {
                battle_result_t r;
                memcpy(&r, data, sizeof(r));
                handleResult(mac, r);
            }
            break;
        case MSG_GIFT_ITEM:
            if (len == sizeof(gift_item_t)) {
                gift_item_t g;
                memcpy(&g, data, sizeof(g));
                handleGiftItem(mac, g);
            }
            break;
        case MSG_VISIT_RECALL:
            if (len == sizeof(visit_recall_t)) {
                visit_recall_t r;
                memcpy(&r, data, sizeof(r));
                handleVisitRecall(mac, r);
            }
            break;
        case MSG_VISIT_PING:
            if (len == sizeof(visit_ping_t)) {
                visit_ping_t p;
                memcpy(&p, data, sizeof(p));
                handleVisitPing(mac, p);
            }
            break;
        case MSG_VISIT_STATUS:
            if (len == sizeof(visit_status_t)) {
                visit_status_t s;
                memcpy(&s, data, sizeof(s));
                handleVisitStatus(mac, s);
            }
            break;
        case MSG_VISIT_INTENT:
            if (len == sizeof(visit_intent_t)) {
                visit_intent_t i;
                memcpy(&i, data, sizeof(i));
                handleVisitIntent(mac, i);
            }
            break;
        case MSG_VISIT_EAT_RESULT:
            if (len == sizeof(visit_eat_result_t)) {
                visit_eat_result_t r;
                memcpy(&r, data, sizeof(r));
                handleVisitEatResult(mac, r);
            }
            break;
        case MSG_ACK:
            if (len == sizeof(ack_msg_t)) {
                ack_msg_t a;
                memcpy(&a, data, sizeof(a));
                handleAck(mac, a);
            }
            break;
    }
}

// ============================================================
// 消息处理
// ============================================================
void BattleLink::handleRoomAdvert(const uint8_t* mac, const room_advert_t& advert) {
    uint8_t myMac[6];
    getMyMac(myMac);
    if (memcmp(advert.mac, myMac, 6) == 0) {
        if (BATTLE_LINK_PACKET_LOGS) {
            Serial.printf("[BattleLink] drop advert self room=%u purpose=%u\n",
                          advert.room_id, advert.purpose);
        }
        return;
    }
    if (roomState != RoomState::SEARCHING) {
        if (BATTLE_LINK_PACKET_LOGS) {
            Serial.printf("[BattleLink] drop advert not searching state=%d room=%u purpose=%u expectedPurpose=%u from=%02X:%02X\n",
                          (int)roomState, advert.room_id, advert.purpose, roomPurpose,
                          mac[0], mac[5]);
        }
        return;
    }
    if (advert.purpose != roomPurpose) {
        BATTLE_LINK_LOG_EVERY_MS(
            BATTLE_LINK_WARN_LOG_INTERVAL_MS,
            Serial.printf("[BattleLink] advert purpose mismatch got=%u expected=%u room=%u from=%02X:%02X\n",
                          advert.purpose, roomPurpose, advert.room_id, mac[0], mac[5]));
        return;
    }
    if (BATTLE_LINK_PACKET_LOGS) {
        Serial.printf("[BattleLink] room advert from %02X:%02X:%02X:%02X:%02X:%02X room=%u purpose=%u\n",
                      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                      advert.room_id, advert.purpose);
    }
    addOrUpdateRoom(advert.mac, advert.room_id);
}

void BattleLink::handleJoinReq(const uint8_t* mac, const join_req_t& req) {
    uint8_t myMac[6];
    getMyMac(myMac);
    if (memcmp(req.mac, myMac, 6) == 0) {
        if (BATTLE_LINK_PACKET_LOGS) {
            Serial.printf("[BattleLink] drop join req self room=%u purpose=%u\n",
                          req.room_id, req.purpose);
        }
        return;
    }
    if (roomState != RoomState::HOSTING) {
        BATTLE_LINK_LOG_EVERY_MS(
            BATTLE_LINK_WARN_LOG_INTERVAL_MS,
            Serial.printf("[BattleLink] join req not hosting state=%d reqRoom=%u hosted=%u reqPurpose=%u roomPurpose=%u from=%02X:%02X\n",
                          (int)roomState, req.room_id, hostedRoomId,
                          req.purpose, roomPurpose, mac[0], mac[5]));
        return;
    }
    if (req.room_id != hostedRoomId) {
        BATTLE_LINK_LOG_EVERY_MS(
            BATTLE_LINK_WARN_LOG_INTERVAL_MS,
            Serial.printf("[BattleLink] join req room mismatch req=%u hosted=%u purpose=%u from=%02X:%02X\n",
                          req.room_id, hostedRoomId, req.purpose, mac[0], mac[5]));
        return;
    }
    if (req.purpose != roomPurpose) {
        BATTLE_LINK_LOG_EVERY_MS(
            BATTLE_LINK_WARN_LOG_INTERVAL_MS,
            Serial.printf("[BattleLink] join req purpose mismatch req=%u hosted=%u room=%u from=%02X:%02X\n",
                          req.purpose, roomPurpose, req.room_id, mac[0], mac[5]));
        return;
    }
    if (pendingJoinReq) {
        BATTLE_LINK_LOG_EVERY_MS(
            BATTLE_LINK_WARN_LOG_INTERVAL_MS,
            Serial.printf("[BattleLink] join req pending exists room=%u purpose=%u from=%02X:%02X\n",
                          req.room_id, req.purpose, mac[0], mac[5]));
        return;
    }
    pendingJoinReq = true;
    pendingJoinReqData = req;
    Serial.printf("[BattleLink] join req accepted room=%u purpose=%u from=%02X:%02X:%02X:%02X:%02X:%02X reqMac=%02X:%02X:%02X:%02X:%02X:%02X\n",
                  req.room_id, req.purpose,
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                  req.mac[0], req.mac[1], req.mac[2], req.mac[3], req.mac[4], req.mac[5]);
}

void BattleLink::handleJoinAck(const uint8_t* mac, const join_ack_t& ack) {
    if (roomState != RoomState::SEARCHING) {
        Serial.printf("[BattleLink] drop join ack not searching state=%d accepted=%u from=%02X:%02X\n",
                      (int)roomState, ack.accepted, mac[0], mac[5]);
        return;
    }
    uint8_t myMac[6];
    getMyMac(myMac);
    if (memcmp(ack.client_mac, myMac, 6) != 0) {
        Serial.printf("[BattleLink] drop join ack client mismatch accepted=%u from=%02X:%02X client=%02X:%02X:%02X:%02X:%02X:%02X my=%02X:%02X:%02X:%02X:%02X:%02X\n",
                      ack.accepted, mac[0], mac[5],
                      ack.client_mac[0], ack.client_mac[1], ack.client_mac[2],
                      ack.client_mac[3], ack.client_mac[4], ack.client_mac[5],
                      myMac[0], myMac[1], myMac[2],
                      myMac[3], myMac[4], myMac[5]);
        return;
    }
    pendingJoinAck = true;
    pendingJoinAckData = ack;
    Serial.printf("[BattleLink] join ack accepted=%u from=%02X:%02X:%02X:%02X:%02X:%02X host=%02X:%02X:%02X:%02X:%02X:%02X\n",
                  ack.accepted,
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                  ack.host_mac[0], ack.host_mac[1], ack.host_mac[2],
                  ack.host_mac[3], ack.host_mac[4], ack.host_mac[5]);
}

void BattleLink::handleReady(const uint8_t* mac, const battle_ready_t& ready) {
    if (!isBattlePeerMac(mac)) return;
    if (ready.version != BATTLE_PROTOCOL_VERSION) {
        Serial.printf("[BattleLink] ready version mismatch got=%d expected=%d\n",
                      ready.version, BATTLE_PROTOCOL_VERSION);
        return;
    }
    pendingReady = true;
    pendingReadyData = ready;
    queueAck(mac, MSG_BATTLE_READY);
    Serial.printf("[BattleLink] rhythm ready received: round=%d mot=%d\n",
                  ready.round_num, ready.my_mot);
}

void BattleLink::handleSync(const uint8_t* mac, const battle_sync_t& sync) {
    if (!isBattlePeerMac(mac)) return;
    if (sync.version != BATTLE_PROTOCOL_VERSION) {
        Serial.printf("[BattleLink] sync version mismatch got=%d expected=%d\n",
                      sync.version, BATTLE_PROTOCOL_VERSION);
        return;
    }
    pendingSync = true;
    pendingSyncData = sync;
    queueAck(mac, MSG_BATTLE_SYNC);
    Serial.printf("[BattleLink] sync received: siz=%d str=%d end=%d spi=%d spd=%d mot=%d strCap=%d temp=%d\n",
                  sync.siz, sync.str, sync.end, sync.spi, sync.spd, sync.motivation,
                  sync.str_cap, sync.temperament);
}

void BattleLink::handleRound(const uint8_t* mac, const battle_round_t& round) {
    if (!isBattlePeerMac(mac)) return;
    if (round.version != BATTLE_PROTOCOL_VERSION) {
        Serial.printf("[BattleLink] round version mismatch got=%d expected=%d\n",
                      round.version, BATTLE_PROTOCOL_VERSION);
        return;
    }
    pendingRound = true;
    pendingRoundData = round;
    queueAck(mac, MSG_BATTLE_ROUND);
    Serial.printf("[BattleLink] round %d received: hDmg=%d cDmg=%d hHp=%d cHp=%d hGauge=%d cGauge=%d\n",
                  round.round_num, round.host_dmg, round.client_dmg, round.host_hp, round.client_hp,
                  round.host_gauge, round.client_gauge);
}

void BattleLink::handleResult(const uint8_t* mac, const battle_result_t& result) {
    if (!isBattlePeerMac(mac)) return;
    if (result.version != BATTLE_PROTOCOL_VERSION) {
        Serial.printf("[BattleLink] result version mismatch got=%d expected=%d\n",
                      result.version, BATTLE_PROTOCOL_VERSION);
        return;
    }
    pendingResult = true;
    pendingResultWin = (result.win != 0);
    queueAck(mac, MSG_BATTLE_RESULT);
    Serial.printf("[BattleLink] result received: win=%d\n", result.win);
}

void BattleLink::handleGiftItem(const uint8_t* mac, const gift_item_t& gift) {
    if (!isBattlePeerMac(mac)) return;
    if (gift.version != BATTLE_PROTOCOL_VERSION) {
        Serial.printf("[BattleLink] gift version mismatch got=%d expected=%d\n",
                      gift.version, BATTLE_PROTOCOL_VERSION);
        return;
    }
    if (gift.amount == 0) return;

    bool duplicate = lastGiftSeen &&
                     lastGiftTransferId == gift.transfer_id &&
                     memcmp(lastGiftPeerMac, mac, 6) == 0;
    queueAck(mac, MSG_GIFT_ITEM);
    if (duplicate) {
        Serial.printf("[BattleLink] duplicate gift tx=%u acked\n", gift.transfer_id);
        return;
    }

    lastGiftSeen = true;
    lastGiftTransferId = gift.transfer_id;
    memcpy(lastGiftPeerMac, mac, 6);
    pendingGift = true;
    pendingGiftData = gift;
    Serial.printf("[BattleLink] gift received tx=%u id=%u amount=%u\n",
                  gift.transfer_id, gift.item_id, gift.amount);
}

void BattleLink::handleVisitRecall(const uint8_t* mac, const visit_recall_t& recall) {
    if (!isBattlePeerMac(mac)) return;
    if (recall.version != BATTLE_PROTOCOL_VERSION) {
        BATTLE_LINK_LOG_EVERY_MS(
            BATTLE_LINK_WARN_LOG_INTERVAL_MS,
            Serial.printf("[BattleLink] visit recall version mismatch got=%d expected=%d\n",
                          recall.version, BATTLE_PROTOCOL_VERSION));
        return;
    }
    pendingVisitRecall = true;
    queueAck(mac, MSG_VISIT_RECALL);
    Serial.println("[BattleLink] visit recall received");
}

void BattleLink::handleVisitPing(const uint8_t* mac, const visit_ping_t& ping) {
    if (!isBattlePeerMac(mac)) return;
    if (ping.version != BATTLE_PROTOCOL_VERSION) {
        BATTLE_LINK_LOG_EVERY_MS(
            BATTLE_LINK_WARN_LOG_INTERVAL_MS,
            Serial.printf("[BattleLink] visit ping version mismatch got=%d expected=%d\n",
                          ping.version, BATTLE_PROTOCOL_VERSION));
        return;
    }
    pendingVisitHunger = ping.hunger > 100 ? 100 : ping.hunger;
    pendingVisitMotivation = ping.motivation > 100 ? 100 : ping.motivation;
    pendingVisitVitals = true;
    queueAck(mac, MSG_VISIT_PING);
}

void BattleLink::handleVisitStatus(const uint8_t* mac, const visit_status_t& status) {
    if (!isBattlePeerMac(mac)) return;
    if (status.version != BATTLE_PROTOCOL_VERSION) {
        BATTLE_LINK_LOG_EVERY_MS(
            BATTLE_LINK_WARN_LOG_INTERVAL_MS,
            Serial.printf("[BattleLink] visit status version mismatch got=%d expected=%d\n",
                          status.version, BATTLE_PROTOCOL_VERSION));
        return;
    }
    pendingVisitStatus = true;
    pendingVisitStatusData = status;
    queueAck(mac, MSG_VISIT_STATUS);
    if (BATTLE_LINK_PACKET_LOGS) {
        Serial.printf("[BattleLink] visit status received active=%u remain=%u duration=%u speed=%u\n",
                      status.flags & 1, status.remaining_s, status.duration_s, status.speed_x10);
    }
}

void BattleLink::handleVisitIntent(const uint8_t* mac, const visit_intent_t& intent) {
    if (!isBattlePeerMac(mac)) return;
    if (intent.version != BATTLE_PROTOCOL_VERSION) {
        BATTLE_LINK_LOG_EVERY_MS(
            BATTLE_LINK_WARN_LOG_INTERVAL_MS,
            Serial.printf("[BattleLink] visit intent version mismatch got=%d expected=%d\n",
                          intent.version, BATTLE_PROTOCOL_VERSION));
        return;
    }
    pendingVisitIntent = true;
    pendingVisitIntentData = intent.intent;
    queueAck(mac, MSG_VISIT_INTENT);
    if (BATTLE_LINK_PACKET_LOGS) {
        Serial.printf("[BattleLink] visit intent received=%u\n", intent.intent);
    }
}

void BattleLink::handleVisitEatResult(const uint8_t* mac, const visit_eat_result_t& result) {
    if (!isBattlePeerMac(mac)) return;
    if (result.version != BATTLE_PROTOCOL_VERSION) {
        BATTLE_LINK_LOG_EVERY_MS(
            BATTLE_LINK_WARN_LOG_INTERVAL_MS,
            Serial.printf("[BattleLink] visit eat result version mismatch got=%d expected=%d\n",
                          result.version, BATTLE_PROTOCOL_VERSION));
        return;
    }
    pendingVisitEatResult = true;
    pendingVisitEatResultData = result;
    queueAck(mac, MSG_VISIT_EAT_RESULT);
    if (BATTLE_LINK_PACKET_LOGS) {
        Serial.printf("[BattleLink] visit eat result received success=%u gain=%u hun=%u food=%u\n",
                      result.success, result.hunger_gain, result.new_guest_hunger, result.food_type);
    }
}

void BattleLink::handleAck(const uint8_t* mac, const ack_msg_t& ack) {
    if (!isBattlePeerMac(mac)) return;
    if (sendState != SendState::SENDING) return;
    if (ack.acked_type == sendCtx.buf[0]) {
        sendCtx.ackReceived = true;
        sendState = SendState::SUCCESS;
        if (BATTLE_LINK_PACKET_LOGS) Serial.println("[BattleLink] ack received");
    }
}

// ============================================================
// 轮询
// ============================================================
void BattleLink::update() {
    uint32_t now = Hal::ins().millis();

    // 房主广播房间
    if (roomState == RoomState::HOSTING) {
        if (now - lastRoomAdvertMs >= ROOM_ADVERT_INTERVAL_MS) {
            if (!sendBusy) {
                ensureBroadcastPeer();
                room_advert_t advert;
                advert.type = MSG_ROOM_ADVERT;
                getMyMac(advert.mac);
                advert.room_id = hostedRoomId;
                advert.purpose = roomPurpose;
                sendBusy = true;
                esp_err_t err = esp_now_send(BROADCAST_MAC, (uint8_t*)&advert, sizeof(advert));
                if (err != ESP_OK) {
                    sendBusy = false;
                    BATTLE_LINK_LOG_EVERY_MS(
                        BATTLE_LINK_WARN_LOG_INTERVAL_MS,
                        Serial.printf("[BattleLink] room advert failed room=%u purpose=%u err=%d\n",
                                      hostedRoomId, roomPurpose, err));
                }
                lastRoomAdvertMs = now;
            }
        }
    }

    // 搜索者清理过期房间
    if (roomState == RoomState::SEARCHING) {
        expireRooms();
    }

    // ACK 超时重试
    if (sendState == SendState::SENDING) {
        if (sendCtx.ackReceived) {
            sendState = SendState::SUCCESS;
        } else if (now - sendCtx.startMs > ACK_TIMEOUT_MS) {
            sendCtx.retryCount++;
            if (sendCtx.retryCount > MAX_RETRIES) {
                sendState = SendState::FAIL;
                BATTLE_LINK_LOG_EVERY_MS(BATTLE_LINK_WARN_LOG_INTERVAL_MS,
                                         Serial.println("[BattleLink] send retries exhausted"));
            } else {
                sendCtx.startMs = now;
                sendInternal(sendCtx.targetMac, sendCtx.buf, sendCtx.len);
                if (BATTLE_LINK_PACKET_LOGS) {
                    Serial.printf("[BattleLink] retry %d/%d\n", sendCtx.retryCount, MAX_RETRIES);
                }
            }
        }
    }

    // 发送排队 ACK
    if (pendingAck && !sendBusy) {
        if (sendAckPacket(pendingAckMac, pendingAckType)) {
            pendingAck = false;
        }
    }
}

// ============================================================
// 房间列表管理
// ============================================================
void BattleLink::addOrUpdateRoom(const uint8_t* mac, uint8_t roomId) {
    uint32_t now = Hal::ins().millis();
    // 查找是否已存在
    for (uint8_t i = 0; i < MAX_ROOM_LIST; i++) {
        if (roomList[i].lastSeenMs != 0 && memcmp(roomList[i].mac, mac, 6) == 0) {
            roomList[i].room_id = roomId;
            roomList[i].purpose = roomPurpose;
            roomList[i].lastSeenMs = now;
            return;
        }
    }
    // 找空位或最旧的条目
    uint8_t slot = 0xFF;
    uint32_t oldest = now;
    for (uint8_t i = 0; i < MAX_ROOM_LIST; i++) {
        if (roomList[i].lastSeenMs == 0) {
            slot = i;
            break;
        }
        if (roomList[i].lastSeenMs < oldest) {
            oldest = roomList[i].lastSeenMs;
            slot = i;
        }
    }
    if (slot < MAX_ROOM_LIST) {
        memcpy(roomList[slot].mac, mac, 6);
        roomList[slot].room_id = roomId;
        roomList[slot].purpose = roomPurpose;
        roomList[slot].lastSeenMs = now;
    }
}

void BattleLink::expireRooms() {
    uint32_t now = Hal::ins().millis();
    for (uint8_t i = 0; i < MAX_ROOM_LIST; i++) {
        if (roomList[i].lastSeenMs != 0 && now - roomList[i].lastSeenMs >= ROOM_ENTRY_TIMEOUT_MS) {
            roomList[i].lastSeenMs = 0;
        }
    }
}

void BattleLink::queueAck(const uint8_t mac[6], uint8_t ackedType) {
    pendingAck = true;
    memcpy(pendingAckMac, mac, 6);
    pendingAckType = ackedType;
    if (BATTLE_LINK_PACKET_LOGS) {
        Serial.printf("[BattleLink] ack queued for type=%d\n", ackedType);
    }
}

bool BattleLink::isBattlePeerMac(const uint8_t mac[6]) const {
    return battlePeerSet && memcmp(mac, battlePeerMac, 6) == 0;
}
