#include "BattleLink.h"
#include "Hal.h"
#include <WiFi.h>
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
    sendState = SendState::IDLE;
    Serial.println("[BattleLink] initialized");
    return true;
}

void BattleLink::end() {
    if (!initialized) return;
    Serial.println("[BattleLink] end()");
    stopDiscovery();
    esp_now_unregister_recv_cb();
    esp_now_unregister_send_cb();
    esp_now_del_peer(BROADCAST_MAC);
    esp_now_deinit();
    WiFi.mode(WIFI_OFF);
    initialized = false;
    sendBusy = false;
    sendState = SendState::IDLE;
    peerConnected = false;
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
// 发现
// ============================================================
void BattleLink::startDiscovery() {
    if (!initialized) begin();
    discoveryActive = true;
    connectFailed = false;
    peerConnected = false;
    discoveryStartMs = Hal::ins().millis();
    lastDiscoverBroadcastMs = 0;
    memset(peerMac, 0, 6);
    pendingSync = false;
    pendingRound = false;
    pendingResult = false;
    pendingAck = false;
    sendState = SendState::IDLE;
    Serial.println("[BattleLink] discovery started");
}

void BattleLink::stopDiscovery() {
    discoveryActive = false;
    Serial.println("[BattleLink] discovery stopped");
}

// ============================================================
// 发送 API
// ============================================================
bool BattleLink::sendSync(const battle_sync_t& sync) {
    if (!peerConnected) { Serial.println("[BattleLink] sendSync: not connected"); return false; }
    if (sendState == SendState::SENDING) { Serial.println("[BattleLink] sendSync: busy"); return false; }
    memcpy(sendCtx.buf, &sync, sizeof(sync));
    sendCtx.len = sizeof(sync);
    memcpy(sendCtx.targetMac, peerMac, 6);
    sendCtx.retryCount = 0;
    sendCtx.ackReceived = false;
    sendCtx.startMs = Hal::ins().millis();
    sendState = SendState::SENDING;
    Serial.println("[BattleLink] sendSync");
    return sendInternal(peerMac, sendCtx.buf, sendCtx.len);
}

bool BattleLink::sendRound(const battle_round_t& round) {
    if (!peerConnected) { Serial.println("[BattleLink] sendRound: not connected"); return false; }
    if (sendState == SendState::SENDING) { Serial.println("[BattleLink] sendRound: busy"); return false; }
    memcpy(sendCtx.buf, &round, sizeof(round));
    sendCtx.len = sizeof(round);
    memcpy(sendCtx.targetMac, peerMac, 6);
    sendCtx.retryCount = 0;
    sendCtx.ackReceived = false;
    sendCtx.startMs = Hal::ins().millis();
    sendState = SendState::SENDING;
    Serial.println("[BattleLink] sendRound");
    return sendInternal(peerMac, sendCtx.buf, sendCtx.len);
}

bool BattleLink::sendResult(bool win) {
    if (!peerConnected) { Serial.println("[BattleLink] sendResult: not connected"); return false; }
    if (sendState == SendState::SENDING) { Serial.println("[BattleLink] sendResult: busy"); return false; }
    battle_result_t res = { MSG_BATTLE_RESULT, win ? (uint8_t)1 : (uint8_t)0 };
    memcpy(sendCtx.buf, &res, sizeof(res));
    sendCtx.len = sizeof(res);
    memcpy(sendCtx.targetMac, peerMac, 6);
    sendCtx.retryCount = 0;
    sendCtx.ackReceived = false;
    sendCtx.startMs = Hal::ins().millis();
    sendState = SendState::SENDING;
    Serial.printf("[BattleLink] sendResult win=%d\n", win);
    return sendInternal(peerMac, sendCtx.buf, sendCtx.len);
}

bool BattleLink::sendInternal(const uint8_t mac[6], const uint8_t* data, uint8_t len) {
    if (!initialized || sendBusy) return false;
    ensureUnicastPeer(mac);
    sendBusy = true;
    esp_err_t err = esp_now_send(mac, data, len);
    if (err != ESP_OK) {
        sendBusy = false;
        Serial.printf("[BattleLink] send failed, err=%d\n", err);
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
    sendBusy = false;
    Serial.printf("[BattleLink] sent to %02X:%02X:%02X:%02X:%02X:%02X status=%s\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                  status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");

    if (status != ESP_NOW_SEND_SUCCESS && sendState == SendState::SENDING) {
        sendCtx.retryCount++;
        if (sendCtx.retryCount > MAX_RETRIES) {
            sendState = SendState::FAIL;
        }
    }
}

void BattleLink::onRecv(const uint8_t* mac, const uint8_t* data, int len) {
    if (len < 2) return;
    uint8_t type = data[0];

    switch (type) {
        case MSG_DISCOVER:
            handleDiscover(mac, data, (uint8_t)len);
            break;
        case MSG_DISCOVER_ACK:
            handleDiscoverAck(mac, data, (uint8_t)len);
            break;
        case MSG_BATTLE_SYNC:
            if (len == sizeof(battle_sync_t)) {
                battle_sync_t s;
                memcpy(&s, data, sizeof(s));
                handleSync(mac, s);
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

void BattleLink::handleDiscover(const uint8_t* mac, const uint8_t* data, uint8_t len) {
    if (len != sizeof(discover_msg_t)) return;
    discover_msg_t d;
    memcpy(&d, data, sizeof(d));

    // 忽略本机
    uint8_t myMac[6];
    WiFi.macAddress(myMac);
    if (memcmp(d.mac, myMac, 6) == 0) return;

    Serial.printf("[BattleLink] discover from %02X...%02X\n", mac[0], mac[5]);

    // 回复 ACK
    discover_msg_t ack = { MSG_DISCOVER_ACK, {0} };
    WiFi.macAddress(ack.mac);
    sendInternal(mac, (uint8_t*)&ack, sizeof(ack));

    // 如果还没连接，把这个设备作为对手
    if (!peerConnected) {
        memcpy(peerMac, mac, 6);
        peerConnected = true;
        discoveryActive = false;
        Serial.println("[BattleLink] peer connected (as responder)");
    }
}

void BattleLink::handleDiscoverAck(const uint8_t* mac, const uint8_t* data, uint8_t len) {
    if (len != sizeof(discover_msg_t)) return;
    if (peerConnected) return;

    discover_msg_t d;
    memcpy(&d, data, sizeof(d));

    uint8_t myMac[6];
    WiFi.macAddress(myMac);
    if (memcmp(d.mac, myMac, 6) == 0) return;

    memcpy(peerMac, mac, 6);
    peerConnected = true;
    discoveryActive = false;
    Serial.printf("[BattleLink] peer connected (as initiator) %02X...%02X\n", mac[0], mac[5]);
}

void BattleLink::handleSync(const uint8_t* mac, const battle_sync_t& sync) {
    (void)mac;
    pendingSync = true;
    pendingSyncData = sync;
    queueAck(mac, MSG_BATTLE_SYNC);
    Serial.printf("[BattleLink] sync received: siz=%d str=%d end=%d spi=%d mot=%d\n",
                  sync.siz, sync.str, sync.end, sync.spi, sync.motivation);
}

void BattleLink::handleRound(const uint8_t* mac, const battle_round_t& round) {
    (void)mac;
    pendingRound = true;
    pendingRoundData = round;
    queueAck(mac, MSG_BATTLE_ROUND);
    Serial.printf("[BattleLink] round %d received: dmg=%d hp=%d mot=%d crit=%d\n",
                  round.round_num, round.my_dmg, round.my_hp, round.my_mot, round.my_crit);
}

void BattleLink::handleResult(const uint8_t* mac, const battle_result_t& result) {
    (void)mac;
    pendingResult = true;
    pendingResultWin = (result.win != 0);
    queueAck(mac, MSG_BATTLE_RESULT);
    Serial.printf("[BattleLink] result received: win=%d\n", result.win);
}

void BattleLink::handleAck(const uint8_t* mac, const ack_msg_t& ack) {
    (void)mac;
    if (sendState != SendState::SENDING) return;
    if (ack.acked_type == sendCtx.buf[0]) {
        sendCtx.ackReceived = true;
        sendState = SendState::SUCCESS;
        Serial.println("[BattleLink] ack received");
    }
}

void BattleLink::queueAck(const uint8_t mac[6], uint8_t ackedType) {
    pendingAck = true;
    memcpy(pendingAckMac, mac, 6);
    pendingAckType = ackedType;
    Serial.printf("[BattleLink] ack queued for type=%d\n", ackedType);
}

// ============================================================
// 轮询
// ============================================================
void BattleLink::update() {
    uint32_t now = Hal::ins().millis();

    // 发现广播
    if (discoveryActive) {
        if (now - discoveryStartMs > DISCOVER_TIMEOUT_MS) {
            connectFailed = true;
            discoveryActive = false;
            Serial.println("[BattleLink] discovery timeout");
        } else if (now - lastDiscoverBroadcastMs > DISCOVER_INTERVAL_MS) {
            if (!sendBusy) {
                ensureBroadcastPeer();
                discover_msg_t d = { MSG_DISCOVER, {0} };
                WiFi.macAddress(d.mac);
                sendBusy = true;
                esp_err_t err = esp_now_send(BROADCAST_MAC, (uint8_t*)&d, sizeof(d));
                if (err != ESP_OK) sendBusy = false;
                lastDiscoverBroadcastMs = now;
            }
        }
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
        ack_msg_t ack = { MSG_ACK, pendingAckType };
        sendInternal(pendingAckMac, (uint8_t*)&ack, sizeof(ack));
        pendingAck = false;
    }
}
