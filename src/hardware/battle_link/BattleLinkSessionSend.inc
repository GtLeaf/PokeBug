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
    visitPowerSaveActive = false;

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
    visitPowerSaveActive = false;
    sendState = SendState::IDLE;
    battlePeerSet = false;
    pendingGift = false;
    lastGiftSeen = false;
    pendingVisitVitals = false;
}

void BattleLink::setVisitPowerSave(bool enabled) {
    if (!initialized) {
        visitPowerSaveActive = false;
        return;
    }
    if (enabled == visitPowerSaveActive) return;

    esp_err_t err = esp_wifi_set_ps(enabled ? WIFI_PS_MIN_MODEM : WIFI_PS_NONE);
    if (err != ESP_OK) {
        Serial.printf("[BattleLink] visit WiFi PS set failed enabled=%u err=%d\n",
                      enabled ? 1 : 0, err);
        return;
    }
    visitPowerSaveActive = enabled;
    Serial.printf("[BattleLink] visit WiFi PS %s\n", enabled ? "MIN_MODEM" : "NONE");
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
    setVisitPowerSave(false);
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
    setVisitPowerSave(false);
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

bool BattleLink::sendVisitStatus(uint32_t remainingMs, uint32_t durationMs, uint8_t speedX10, bool active,
                                 uint8_t guestEatCount, uint8_t guestPlayCount) {
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
        guestEatCount,
        guestPlayCount,
    };
    memcpy(sendCtx.buf, &status, sizeof(status));
    sendCtx.len = sizeof(status);
    memcpy(sendCtx.targetMac, battlePeerMac, 6);
    sendCtx.retryCount = 0;
    sendCtx.ackReceived = false;
    sendCtx.startMs = Hal::ins().millis();
    if (BATTLE_LINK_PACKET_LOGS) {
        Serial.printf("[BattleLink] sendVisitStatus active=%u remain=%u duration=%u speed=%u eat=%u play=%u\n",
                      status.flags & 1, status.remaining_s, status.duration_s, status.speed_x10,
                      status.guest_eat_count, status.guest_play_count);
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

