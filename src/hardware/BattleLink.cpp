#include "BattleLink.h"
#include "Hal.h"
#include <esp_wifi.h>
#include <cstring>

const uint8_t BattleLink::BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

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
void BattleLink::startRoomHost(uint8_t roomId) {
    if (!initialized) begin();
    roomState = RoomState::HOSTING;
    hostedRoomId = roomId;
    lastRoomAdvertMs = 0;
    pendingJoinReq = false;
    Serial.printf("[BattleLink] start hosting room %d\n", roomId);
}

void BattleLink::startRoomSearch() {
    if (!initialized) begin();
    roomState = RoomState::SEARCHING;
    memset(roomList, 0, sizeof(roomList));
    Serial.println("[BattleLink] start searching rooms");
}

void BattleLink::stopRoom() {
    roomState = RoomState::IDLE;
    hostedRoomId = 0;
    Serial.println("[BattleLink] stop room");
}

bool BattleLink::takeJoinReq(join_req_t& out) {
    if (!pendingJoinReq) return false;
    out = pendingJoinReqData;
    pendingJoinReq = false;
    return true;
}

bool BattleLink::sendJoinAck(bool accept, const uint8_t* clientMac) {
    if (!initialized || !battlePeerSet) {
        Serial.println("[BattleLink] sendJoinAck: not ready");
        return false;
    }
    join_ack_t ack;
    ack.type = MSG_JOIN_ACK;
    getMyMac(ack.host_mac);
    memcpy(ack.client_mac, clientMac, 6);
    ack.accepted = accept ? 1 : 0;
    Serial.printf("[BattleLink] sendJoinAck accept=%d\n", accept ? 1 : 0);
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
    Serial.printf("[BattleLink] sendJoinReq room=%d host=%02X:%02X\n",
                  roomId, hostMac[0], hostMac[5]);
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
    Serial.printf("[BattleLink] sendReady round=%d mot=%d\n", ready.round_num, ready.my_mot);
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
    Serial.printf("[BattleLink] sendRound hDmg=%d cDmg=%d hHp=%d cHp=%d\n",
                  round.host_dmg, round.client_dmg, round.host_hp, round.client_hp);
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

bool BattleLink::sendInternal(const uint8_t mac[6], const uint8_t* data, uint8_t len) {
    if (!initialized || sendBusy) return false;
    ensureUnicastPeer(mac);
    sendBusy = true;
    currentSendTracked = true;
    esp_err_t err = esp_now_send(mac, data, len);
    if (err != ESP_OK) {
        sendBusy = false;
        currentSendTracked = false;
        Serial.printf("[BattleLink] send failed, err=%d\n", err);
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
        Serial.printf("[BattleLink] ack send failed, err=%d\n", err);
        return false;
    }
    return true;
}

bool BattleLink::sendLobbyPacket(const uint8_t mac[6], const uint8_t* data, uint8_t len) {
    if (!initialized) return false;
    ensureUnicastPeer(mac);
    esp_err_t err = esp_now_send(mac, data, len);
    if (err != ESP_OK) {
        Serial.printf("[BattleLink] lobby send failed, err=%d\n", err);
        return false;
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
    Serial.printf("[BattleLink] sent to %02X:%02X:%02X:%02X:%02X:%02X status=%s\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                  status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");

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
    if (memcmp(advert.mac, myMac, 6) == 0) return;
    if (roomState != RoomState::SEARCHING) return;
    Serial.printf("[BattleLink] room advert from %02X:%02X room=%d\n",
                  mac[0], mac[5], advert.room_id);
    addOrUpdateRoom(advert.mac, advert.room_id);
}

void BattleLink::handleJoinReq(const uint8_t* mac, const join_req_t& req) {
    uint8_t myMac[6];
    getMyMac(myMac);
    if (memcmp(req.mac, myMac, 6) == 0) return;
    if (roomState != RoomState::HOSTING) return;
    if (req.room_id != hostedRoomId) return;
    if (pendingJoinReq) return;  // 只保留一个请求
    pendingJoinReq = true;
    pendingJoinReqData = req;
    Serial.printf("[BattleLink] join req for room=%d from %02X:%02X\n",
                  req.room_id, mac[0], mac[5]);
}

void BattleLink::handleJoinAck(const uint8_t* mac, const join_ack_t& ack) {
    (void)mac;
    if (roomState != RoomState::SEARCHING) return;
    pendingJoinAck = true;
    pendingJoinAckData = ack;
    Serial.printf("[BattleLink] join ack accepted=%d\n", ack.accepted);
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
    Serial.printf("[BattleLink] ready received: round=%d mot=%d\n", ready.round_num, ready.my_mot);
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
    Serial.printf("[BattleLink] sync received: siz=%d str=%d end=%d spi=%d spd=%d mot=%d\n",
                  sync.siz, sync.str, sync.end, sync.spi, sync.spd, sync.motivation);
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
    Serial.printf("[BattleLink] round %d received: hDmg=%d cDmg=%d hHp=%d cHp=%d\n",
                  round.round_num, round.host_dmg, round.client_dmg, round.host_hp, round.client_hp);
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

void BattleLink::handleAck(const uint8_t* mac, const ack_msg_t& ack) {
    if (!isBattlePeerMac(mac)) return;
    if (sendState != SendState::SENDING) return;
    if (ack.acked_type == sendCtx.buf[0]) {
        sendCtx.ackReceived = true;
        sendState = SendState::SUCCESS;
        Serial.println("[BattleLink] ack received");
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
                sendBusy = true;
                esp_err_t err = esp_now_send(BROADCAST_MAC, (uint8_t*)&advert, sizeof(advert));
                if (err != ESP_OK) sendBusy = false;
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
                Serial.println("[BattleLink] send retries exhausted");
            } else {
                sendCtx.startMs = now;
                sendInternal(sendCtx.targetMac, sendCtx.buf, sendCtx.len);
                Serial.printf("[BattleLink] retry %d/%d\n", sendCtx.retryCount, MAX_RETRIES);
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
    Serial.printf("[BattleLink] ack queued for type=%d\n", ackedType);
}

bool BattleLink::isBattlePeerMac(const uint8_t mac[6]) const {
    return battlePeerSet && memcmp(mac, battlePeerMac, 6) == 0;
}
