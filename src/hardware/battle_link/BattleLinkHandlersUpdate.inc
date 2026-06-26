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
        Serial.printf("[BattleLink] visit status received active=%u remain=%u duration=%u speed=%u eat=%u play=%u\n",
                      status.flags & 1, status.remaining_s, status.duration_s, status.speed_x10,
                      status.guest_eat_count, status.guest_play_count);
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
        } else {
            uint32_t ackTimeoutMs = isVisitTrackedMessage(sendCtx.buf[0])
                                    ? VISIT_ACK_TIMEOUT_MS
                                    : ACK_TIMEOUT_MS;
            if (now - sendCtx.startMs > ackTimeoutMs) {
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
