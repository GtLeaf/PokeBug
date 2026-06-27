void LobbyScene::onEnter() {
    state = State::MODE_SELECT;
    purpose = Purpose::BATTLE;
    selectedMode = Mode::NONE;
    roomId = 0;
    stateStartMs = Hal::ins().millis();
    lastScanLogMs = 0;
    selectedRoomIdx = 0;
    roomCount = 0;
    joinTargetRoomId = 0;
    memset(joinTargetMac, 0, sizeof(joinTargetMac));
    lastJoinLogMs = 0;
    messageText = nullptr;
    messageReturnToMenu = false;
    giftSendStarted = false;
    visitSyncSent = false;
    visitSyncAcked = false;
    visitSyncReceived = false;
    messageBuf[0] = '\0';
    BattleLink::ins().begin();

    // 由菜单指定入口模式时直接跳转
    LobbyMode startMode = GameEngine::ins().getLobbyMode();
    GameEngine::ins().setLobbyMode(LobbyMode::LOBBY_DEFAULT);
    Serial.printf("[Lobby] onEnter startMode=%d\n", (int)startMode);
    switch (startMode) {
        case LobbyMode::LOBBY_CREATE:
            enterHostWaiting();
            break;
        case LobbyMode::LOBBY_SEARCH:
            enterSearchScanning();
            break;
        case LobbyMode::LOBBY_GIFT_SEND:
            purpose = Purpose::GIFT_SEND;
            if (validatePendingGift()) {
                enterSearchScanning();
            } else {
                enterMessage(UiStrings::GIFT_NO_FOOD, true);
            }
            break;
        case LobbyMode::LOBBY_GIFT_RECEIVE:
            purpose = Purpose::GIFT_RECEIVE;
            GameEngine::ins().clearPendingGiftItem();
            enterHostWaiting();
            break;
        case LobbyMode::LOBBY_VISIT_CREATE:
            purpose = Purpose::VISIT;
            enterHostWaiting();
            break;
        case LobbyMode::LOBBY_VISIT_SEARCH:
            purpose = Purpose::VISIT;
            enterSearchScanning();
            break;
        default:
            enterModeSelect();
            break;
    }

    Serial.printf("[Lobby] entered purpose=%d state=%d\n", (int)purpose, (int)state);
}

void LobbyScene::onExit() {
    BattleLink::ins().stopRoom();
    Serial.println("[Lobby] exited");
}

SceneID LobbyScene::update() {
    BattleLink::ins().update();
    uint32_t now = Hal::ins().millis();

    switch (state) {
        case State::HOST_WAITING: {
            join_req_t req;
            if (BattleLink::ins().takeJoinReq(req)) {
                Serial.printf("[Lobby] host got join req room=%u purpose=%u from=%02X:%02X:%02X:%02X:%02X:%02X localPurpose=%d\n",
                              req.room_id, req.purpose,
                              req.mac[0], req.mac[1], req.mac[2],
                              req.mac[3], req.mac[4], req.mac[5],
                              (int)purpose);
                // 接受加入，设置对端为主机身份
                BattleLink::ins().setBattlePeer(req.mac, true);
                bool ackQueued = BattleLink::ins().sendJoinAck(true, req.mac);
                Serial.printf("[Lobby] host join ack queued=%d\n", ackQueued ? 1 : 0);
                if (purpose == Purpose::GIFT_RECEIVE) {
                    Serial.printf("[Lobby] gift receiver accepted sender %02X:%02X\n",
                                  req.mac[0], req.mac[5]);
                    BattleLink::ins().stopRoom();
                    enterGiftWaiting();
                    break;
                } else if (purpose == Purpose::VISIT) {
                    Serial.printf("[Lobby] visit accepted guest %02X:%02X\n",
                                  req.mac[0], req.mac[5]);
                    BattleLink::ins().stopRoom();
                    enterVisitSyncing();
                    break;
                } else {
                    Serial.printf("[Lobby] host accepted joiner %02X:%02X, goto battle\n",
                                  req.mac[0], req.mac[5]);
                    nextScene = SCENE_BATTLE;
                    return nextScene;
                }
            }
            if (now - stateStartMs >= HOST_TIMEOUT_MS) {
                Serial.printf("[Lobby] host timeout purpose=%d room=%u elapsed=%lu\n",
                              (int)purpose, roomId, (unsigned long)(now - stateStartMs));
                enterMessage(UiStrings::MSG_NO_ONE_JOINED, isDirectMenuPurpose());
            }
            break;
        }

        case State::SEARCH_SCANNING: {
            if (now - lastScanLogMs >= 1000) {
                uint8_t count = BattleLink::ins().getRoomListCount();
                Serial.printf("[Lobby] scanning purpose=%d found=%d elapsed=%lu\n",
                              (int)purpose, count, (unsigned long)(now - stateStartMs));
                lastScanLogMs = now;
            }
            if (now - stateStartMs >= SEARCH_SCAN_MS) {
                roomCount = BattleLink::ins().getRoomListCount();
                if (roomCount == 0) {
                    enterMessage(UiStrings::LOBBY_NO_ROOMS, isDirectMenuPurpose());
                } else {
                    selectedRoomIdx = 0;
                    enterSearchList();
                }
            }
            break;
        }

        case State::SEARCH_LIST: {
            roomCount = BattleLink::ins().getRoomListCount();
            if (roomCount == 0) {
                // 列表全部过期，重新扫描
                enterSearchScanning();
            } else if (selectedRoomIdx >= roomCount) {
                selectedRoomIdx = roomCount - 1;
            }
            break;
        }

        case State::JOINING: {
            join_ack_t ack;
            if (BattleLink::ins().takeJoinAck(ack)) {
                Serial.printf("[Lobby] join ack received accepted=%u host=%02X:%02X:%02X:%02X:%02X:%02X client=%02X:%02X:%02X:%02X:%02X:%02X targetRoom=%u purpose=%d\n",
                              ack.accepted,
                              ack.host_mac[0], ack.host_mac[1], ack.host_mac[2],
                              ack.host_mac[3], ack.host_mac[4], ack.host_mac[5],
                              ack.client_mac[0], ack.client_mac[1], ack.client_mac[2],
                              ack.client_mac[3], ack.client_mac[4], ack.client_mac[5],
                              joinTargetRoomId, (int)purpose);
                if (ack.accepted) {
                    uint8_t hostMac[6];
                    memcpy(hostMac, ack.host_mac, 6);
                    BattleLink::ins().setBattlePeer(hostMac, false);
                    if (purpose == Purpose::GIFT_SEND) {
                        Serial.printf("[Lobby] joined gift room, host=%02X:%02X\n",
                                      hostMac[0], hostMac[5]);
                        enterGiftSending();
                    } else if (purpose == Purpose::VISIT) {
                        Serial.printf("[Lobby] joined visit room, host=%02X:%02X\n",
                                      hostMac[0], hostMac[5]);
                        enterVisitSyncing();
                    } else {
                        Serial.printf("[Lobby] joined room, host=%02X:%02X, goto battle\n",
                                      hostMac[0], hostMac[5]);
                        nextScene = SCENE_BATTLE;
                        return nextScene;
                    }
                } else {
                    Serial.println("[Lobby] join rejected");
                    enterMessage(UiStrings::MSG_REJECTED, isDirectMenuPurpose());
                }
                break;
            }
            if (now - lastJoinLogMs >= 1000) {
                Serial.printf("[Lobby] joining wait purpose=%d room=%u host=%02X:%02X:%02X:%02X:%02X:%02X elapsed=%lu\n",
                              (int)purpose, joinTargetRoomId,
                              joinTargetMac[0], joinTargetMac[1], joinTargetMac[2],
                              joinTargetMac[3], joinTargetMac[4], joinTargetMac[5],
                              (unsigned long)(now - stateStartMs));
                lastJoinLogMs = now;
            }
            uint32_t joinTimeoutMs = isGiftPurpose() ? GIFT_JOIN_TIMEOUT_MS : JOIN_TIMEOUT_MS;
            if (now - stateStartMs >= joinTimeoutMs) {
                Serial.printf("[Lobby] join timeout purpose=%d room=%u host=%02X:%02X:%02X:%02X:%02X:%02X elapsed=%lu\n",
                              (int)purpose, joinTargetRoomId,
                              joinTargetMac[0], joinTargetMac[1], joinTargetMac[2],
                              joinTargetMac[3], joinTargetMac[4], joinTargetMac[5],
                              (unsigned long)(now - stateStartMs));
                enterMessage(UiStrings::MSG_JOIN_TIMEOUT, isDirectMenuPurpose());
            }
            break;
        }

        case State::GIFT_WAITING: {
            gift_item_t gift;
            if (BattleLink::ins().takeReceivedGift(gift)) {
                if (!ItemCatalog::isExchangeable(gift.item_id)) {
                    enterMessage(UiStrings::GIFT_UNSUPPORTED, true);
                } else if (GameEngine::ins().addItem(gift.item_id, gift.amount)) {
                    GameEngine::ins().forceSave();
                    formatGiftMessage(UiStrings::GIFT_RECEIVED, gift.item_id, gift.amount);
                    enterMessage(messageBuf, true);
                } else {
                    enterMessage(UiStrings::GIFT_UNSUPPORTED, true);
                }
            } else if (now - stateStartMs >= GIFT_TIMEOUT_MS) {
                enterMessage(UiStrings::MSG_JOIN_TIMEOUT, true);
            }
            break;
        }

        case State::GIFT_SENDING: {
            ItemStack gift = GameEngine::ins().getPendingGiftItem();
            if (!giftSendStarted && !BattleLink::ins().isSending()) {
                if (!validatePendingGift()) {
                    enterMessage(UiStrings::GIFT_NO_FOOD, true);
                } else if (BattleLink::ins().sendGiftItem(gift.id, gift.amount)) {
                    giftSendStarted = true;
                }
            } else if (giftSendStarted && !BattleLink::ins().isSending()) {
                bool ok = BattleLink::ins().takeLastSendSuccess();
                if (ok && GameEngine::ins().removeItem(gift.id, gift.amount)) {
                    GameEngine::ins().forceSave();
                    GameEngine::ins().clearPendingGiftItem();
                    formatGiftMessage(UiStrings::GIFT_SENT, gift.id, gift.amount);
                    enterMessage(messageBuf, true);
                } else {
                    enterMessage(UiStrings::GIFT_SEND_FAILED, true);
                }
            }
            if (state == State::GIFT_SENDING && now - stateStartMs >= GIFT_TIMEOUT_MS) {
                enterMessage(UiStrings::GIFT_SEND_FAILED, true);
            }
            break;
        }

        case State::VISIT_SYNCING: {
            if (!visitSyncSent) {
                if (sendVisitSync()) {
                    visitSyncSent = true;
                }
            } else if (!visitSyncAcked && !BattleLink::ins().isSending()) {
                if (BattleLink::ins().takeLastSendSuccess()) {
                    visitSyncAcked = true;
                } else {
                    visitSyncSent = false;
                }
            }

            if (!visitSyncReceived) {
                battle_sync_t sync;
                if (BattleLink::ins().takeReceivedSync(sync)) {
                    bool asHost = BattleLink::ins().isHost();
                    if (!asHost && GameEngine::ins().setGameSpeedFromX10(sync.visit_speed_x10)) {
                        GameEngine::ins().saveSettingsSnapshot();
                        Serial.printf("[Lobby] guest synced host game speed=%u\n",
                                      sync.visit_speed_x10);
                    }
                    GameEngine::ins().startVisitSession(asHost,
                                                        sync.siz,
                                                        sync.palette_id,
                                                        sync.hunger,
                                                        sync.motivation,
                                                        sync.str,
                                                        sync.str_cap,
                                                        sync.temperament,
                                                        asHost
                                                            ? GameEngine::ins().getGameSpeedX10()
                                                            : sync.visit_speed_x10);
                    visitSyncReceived = true;
                    Serial.printf("[Lobby] visit sync received siz=%u palette=%u hun=%u mot=%u host=%d speed=%u\n",
                                  sync.siz, sync.palette_id,
                                  sync.hunger, sync.motivation,
                                  asHost ? 1 : 0,
                                  asHost ? GameEngine::ins().getGameSpeedX10() : sync.visit_speed_x10);
                }
            }

            if (visitSyncAcked && visitSyncReceived) {
                nextScene = SCENE_TERRARIUM;
                return nextScene;
            }
            if (now - stateStartMs >= VISIT_SYNC_TIMEOUT_MS) {
                enterMessage(UiStrings::MSG_JOIN_TIMEOUT, true);
            }
            break;
        }

        case State::MESSAGE: {
            if (now - stateStartMs >= MESSAGE_MS) {
                if (messageReturnToMenu) {
                    nextScene = SCENE_MENU;
                    return nextScene;
                }
                enterModeSelect();
            }
            break;
        }

        default:
            break;
    }

    return nextScene;
}
