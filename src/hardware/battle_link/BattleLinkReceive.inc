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
