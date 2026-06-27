void LobbyScene::render() {
    switch (state) {
        case State::MODE_SELECT:     drawModeSelect(); break;
        case State::HOST_WAITING:    drawHostWaiting(); break;
        case State::SEARCH_SCANNING: drawSearchScanning(); break;
        case State::SEARCH_LIST:     drawSearchList(); break;
        case State::JOINING:         drawJoining(); break;
        case State::GIFT_WAITING:    drawGiftWaiting(); break;
        case State::GIFT_SENDING:    drawGiftSending(); break;
        case State::VISIT_SYNCING:   drawVisitSyncing(); break;
        case State::MESSAGE:         drawMessage(); break;
    }
}

bool LobbyScene::onButton(const ButtonEvent& ev) {
    if (ev.action == BtnAction::LONG_PRESS && ev.btn == 1) {
        nextScene = SCENE_MENU;
        return true;
    }

    if (ev.action == BtnAction::LONG_PRESS && ev.btn == 0) {
        nextScene = SCENE_MENU;
        return true;
    }

    if (ev.action != BtnAction::PRESSED) return false;

    if (state == State::MODE_SELECT) {
        if (ev.btn == 1) {
            selectedMode = nextMode(selectedMode);
            return true;
        }
        if (ev.btn == 0) {
            if (selectedMode == Mode::CREATE || selectedMode == Mode::NONE) {
                enterHostWaiting();
            } else if (selectedMode == Mode::SEARCH) {
                enterSearchScanning();
            } else if (selectedMode == Mode::BACK) {
                nextScene = SCENE_MENU;
            }
            return true;
        }
    } else if (state == State::HOST_WAITING) {
        // 创建房间后按 B 取消，关闭广播并返回上一级。
        if (ev.btn == 1) {
            BattleLink::ins().stopRoom();
            nextScene = SCENE_MENU;
            return true;
        }
    } else if (state == State::SEARCH_SCANNING) {
        // 搜索过程中按 B 取消，关闭扫描并返回社交菜单。
        if (ev.btn == 1) {
            BattleLink::ins().stopRoom();
            nextScene = SCENE_MENU;
            return true;
        }
    } else if (state == State::SEARCH_LIST) {
        if (ev.btn == 1) {
            if (roomCount > 0) {
                selectedRoomIdx++;
                if (selectedRoomIdx >= roomCount) selectedRoomIdx = 0;
            }
            return true;
        }
        if (ev.btn == 0) {
            if (roomCount > 0) {
                enterJoining(selectedRoomIdx);
            }
            return true;
        }
    } else if (state == State::JOINING) {
        if (ev.btn == 1) {
            BattleLink::ins().stopRoom();
            nextScene = SCENE_MENU;
            return true;
        }
    } else if (state == State::GIFT_WAITING) {
        if (ev.btn == 1) {
            BattleLink::ins().stopRoom();
            nextScene = SCENE_MENU;
            return true;
        }
    } else if (state == State::GIFT_SENDING) {
        if (ev.btn == 1 && !giftSendStarted && !BattleLink::ins().isSending()) {
            BattleLink::ins().stopRoom();
            GameEngine::ins().clearPendingGiftItem();
            nextScene = SCENE_MENU;
            return true;
        }
    }

    return false;
}

// ============================================================
// 状态切换
// ============================================================
void LobbyScene::enterModeSelect(Mode defaultMode) {
    state = State::MODE_SELECT;
    purpose = Purpose::BATTLE;
    selectedMode = defaultMode;
    BattleLink::ins().stopRoom();
    stateStartMs = Hal::ins().millis();
    Serial.println("[Lobby] mode select");
}

LobbyScene::Mode LobbyScene::nextMode(Mode m) {
    switch (m) {
        case Mode::SEARCH: return Mode::BACK;
        case Mode::BACK:   return Mode::CREATE;
        default:           return Mode::SEARCH;
    }
}

bool LobbyScene::isGiftPurpose() const {
    return purpose == Purpose::GIFT_SEND || purpose == Purpose::GIFT_RECEIVE;
}

bool LobbyScene::isVisitPurpose() const {
    return purpose == Purpose::VISIT;
}

bool LobbyScene::isDirectMenuPurpose() const {
    return isGiftPurpose() || isVisitPurpose();
}

uint8_t LobbyScene::roomPurpose() const {
    if (isGiftPurpose()) return ROOM_PURPOSE_GIFT;
    if (isVisitPurpose()) return ROOM_PURPOSE_VISIT;
    return ROOM_PURPOSE_BATTLE;
}

bool LobbyScene::validatePendingGift() {
    ItemStack gift = GameEngine::ins().getPendingGiftItem();
    if (gift.amount == 0) return false;
    if (!ItemCatalog::isExchangeable(gift.id)) return false;
    return GameEngine::ins().getItemCount(gift.id) >= gift.amount;
}

void LobbyScene::formatGiftMessage(const char* prefix, uint16_t itemId, uint8_t amount) {
    snprintf(messageBuf, sizeof(messageBuf), "%s %s x%u",
             prefix, ItemCatalog::name(itemId), amount);
}

bool LobbyScene::sendVisitSync() {
    Bug& bug = GameEngine::ins().getBug();
    uint8_t hunger = bug.getHunger();
    battle_sync_t sync = {};
    sync.type = MSG_BATTLE_SYNC;
    sync.siz = (uint8_t)roundf(bug.getSiz());
    sync.str = (uint8_t)roundf(bug.getStr());
    sync.end = (uint8_t)roundf(bug.getEnd());
    sync.spi = (uint8_t)roundf(bug.getSpi());
    sync.spd = (uint8_t)roundf(bug.getSpd());
    sync.motivation = bug.getMot();
    sync.hunger = hunger;
    sync.palette_id = visitPaletteCode(bug.getTemperament(), bug.getAdultDepth());
    sync.visit_speed_x10 = GameEngine::ins().getGameSpeedX10();
    sync.str_cap = bug.getStrCap();
    sync.temperament = (uint8_t)bug.getTemperament();
    return BattleLink::ins().sendSync(sync);
}

void LobbyScene::enterHostWaiting() {
    state = State::HOST_WAITING;
    roomId = (uint8_t)random(1, 256);
    BattleLink::ins().startRoomHost(roomId, roomPurpose());
    stateStartMs = Hal::ins().millis();
    Serial.printf("[Lobby] hosting room=%u purpose=%d menuPurpose=%d\n",
                  roomId, roomPurpose(), (int)purpose);
}

void LobbyScene::enterSearchScanning() {
    state = State::SEARCH_SCANNING;
    BattleLink::ins().startRoomSearch(roomPurpose());
    stateStartMs = Hal::ins().millis();
    lastScanLogMs = 0;
    Serial.printf("[Lobby] start scanning purpose=%d menuPurpose=%d\n",
                  roomPurpose(), (int)purpose);
}

void LobbyScene::enterSearchList() {
    state = State::SEARCH_LIST;
    roomCount = BattleLink::ins().getRoomListCount();
    if (selectedRoomIdx >= roomCount) selectedRoomIdx = 0;
    Serial.printf("[Lobby] show %d rooms\n", roomCount);
    for (uint8_t i = 0; i < roomCount; ++i) {
        const BattleLink::RoomEntry* entry = BattleLink::ins().getRoomListEntry(i);
        if (!entry) continue;
        Serial.printf("[Lobby] room[%u] id=%u purpose=%u mac=%02X:%02X:%02X:%02X:%02X:%02X age=%lu\n",
                      i, entry->room_id, entry->purpose,
                      entry->mac[0], entry->mac[1], entry->mac[2],
                      entry->mac[3], entry->mac[4], entry->mac[5],
                      (unsigned long)(Hal::ins().millis() - entry->lastSeenMs));
    }
}

void LobbyScene::enterJoining(uint8_t idx) {
    const BattleLink::RoomEntry* entry = BattleLink::ins().getRoomListEntry(idx);
    if (!entry) {
        enterSearchScanning();
        return;
    }
    state = State::JOINING;
    joinTargetRoomId = entry->room_id;
    memcpy(joinTargetMac, entry->mac, sizeof(joinTargetMac));
    lastJoinLogMs = 0;
    BattleLink::ins().sendJoinReq(entry->room_id, entry->mac);
    stateStartMs = Hal::ins().millis();
    Serial.printf("[Lobby] joining room=%u idx=%u purpose=%d host=%02X:%02X:%02X:%02X:%02X:%02X\n",
                  entry->room_id, idx, (int)purpose,
                  entry->mac[0], entry->mac[1], entry->mac[2],
                  entry->mac[3], entry->mac[4], entry->mac[5]);
}

void LobbyScene::enterGiftWaiting() {
    state = State::GIFT_WAITING;
    stateStartMs = Hal::ins().millis();
    Serial.println("[Lobby] waiting gift item");
}

void LobbyScene::enterGiftSending() {
    state = State::GIFT_SENDING;
    giftSendStarted = false;
    stateStartMs = Hal::ins().millis();
    Serial.println("[Lobby] sending gift item");
}

void LobbyScene::enterVisitSyncing() {
    state = State::VISIT_SYNCING;
    visitSyncSent = false;
    visitSyncAcked = false;
    visitSyncReceived = false;
    stateStartMs = Hal::ins().millis();
    Serial.println("[Lobby] visit syncing");
}

void LobbyScene::enterMessage(const char* text, bool returnToMenu) {
    state = State::MESSAGE;
    messageText = text;
    messageReturnToMenu = returnToMenu;
    stateStartMs = Hal::ins().millis();
    Serial.printf("[Lobby] message: %s\n", text);
}

// ============================================================
// 绘制
// ============================================================
void LobbyScene::drawSettingsStyleList(const char* title, const char* const* items, uint8_t count, uint8_t cursor) {
    PixelRenderer::fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::BLACK);

    float fs = PixelRenderer::getContentFontScale();
    int rowStep = (int)(14 * fs);
    int startY = 8;
    int sepGap = (int)(4 * fs);

    // 标题
    PixelRenderer::drawPixelText(80, startY, title, PixelRenderer::WHITE, 2);
    int listStartY = startY + 26;

    for (uint8_t i = 0; i < count; i++) {
        int y = listStartY + i * rowStep;
        drawListItem(y, items[i], i == cursor, i == count - 1);
    }
}

void LobbyScene::drawListItem(int y, const char* text, bool selected, bool last) {
    float fs = PixelRenderer::getContentFontScale();
    int rowStep = (int)(14 * fs);
    int sepGap = (int)(4 * fs);
    uint16_t color = selected ? PixelRenderer::YELLOW : PixelRenderer::WHITE;

    if (selected) {
        PixelRenderer::fillRect(4, y, 4, (int)(8 * fs), PixelRenderer::YELLOW);
    }
    PixelRenderer::drawPixelText(14, y, text, color);

    if (!last) {
        PixelRenderer::fillRect(4, y + rowStep - sepGap, Hal::DISPLAY_W - 8, 1, PixelRenderer::GRAY);
    }
}

void LobbyScene::drawModeSelect() {
    const char* items[3] = {
        UiStrings::MENU_FIGHT_CREATE,
        UiStrings::MENU_FIGHT_SEARCH,
        UiStrings::BACK,
    };
    uint8_t cursor = 0;
    if (selectedMode == Mode::SEARCH) cursor = 1;
    else if (selectedMode == Mode::BACK) cursor = 2;
    drawSettingsStyleList(UiStrings::MENU_FIGHT, items, 3, cursor);
}

void LobbyScene::drawHostWaiting() {
    PixelRenderer::fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::BLACK);
    const char* title = isGiftPurpose() ? UiStrings::GIFT_RECEIVE_TITLE :
                        (isVisitPurpose() ? UiStrings::VISIT_TITLE : UiStrings::LOBBY_ROOM);
    PixelRenderer::drawPixelText(80, 25, title, PixelRenderer::WHITE, 2);

    char buf[16];
    snprintf(buf, sizeof(buf), "%03d", roomId);
    PixelRenderer::drawPixelText(90, 55, buf, PixelRenderer::YELLOW, 3);

    uint32_t elapsed = Hal::ins().millis() - stateStartMs;
    int dots = (elapsed / 500) % 4;
    const char* waitText = isGiftPurpose() ? UiStrings::GIFT_WAITING :
                           (isVisitPurpose() ? UiStrings::VISIT_WAITING : UiStrings::LOBBY_WAITING);
    snprintf(buf, sizeof(buf), "%s%.*s", waitText, dots, UiStrings::ELLIPSIS);
    PixelRenderer::drawPixelText(75, 100, buf, PixelRenderer::WHITE, 1);
    PixelRenderer::drawPixelText(6, 122, UiStrings::EXPLORE_CANCEL, PixelRenderer::GRAY, 1);
}

void LobbyScene::drawSearchScanning() {
    PixelRenderer::fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::BLACK);
    const char* text = isGiftPurpose() ? UiStrings::GIFT_RECEIVE_TITLE :
                       (isVisitPurpose() ? UiStrings::VISIT_TITLE : UiStrings::LOBBY_SCANNING);
    PixelRenderer::drawPixelText(70, 50, text, PixelRenderer::WHITE, 2);
    uint32_t elapsed = Hal::ins().millis() - stateStartMs;
    int dots = (elapsed / 500) % 4;
    char buf[12];
    snprintf(buf, sizeof(buf), "%.*s", dots, UiStrings::ELLIPSIS);
    PixelRenderer::drawPixelText(110, 80, buf, PixelRenderer::WHITE, 1);
    PixelRenderer::drawPixelText(6, 122, UiStrings::EXPLORE_CANCEL, PixelRenderer::GRAY, 1);
}

void LobbyScene::drawSearchList() {
    roomCount = BattleLink::ins().getRoomListCount();
    if (roomCount == 0) {
        // 列表为空时回到扫描或提示
        PixelRenderer::fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::BLACK);
        PixelRenderer::drawPixelText(60, 60, UiStrings::LOBBY_NO_ROOMS, PixelRenderer::YELLOW, 1);
        return;
    }

    const char* items[BattleLink::MAX_ROOM_LIST];
    char buf[BattleLink::MAX_ROOM_LIST][16];
    for (uint8_t i = 0; i < roomCount; i++) {
        const BattleLink::RoomEntry* entry = BattleLink::ins().getRoomListEntry(i);
        snprintf(buf[i], sizeof(buf[i]), "%s %03d", UiStrings::LOBBY_ROOM, entry ? entry->room_id : 0);
        items[i] = buf[i];
    }
    drawSettingsStyleList(isGiftPurpose() ? UiStrings::GIFT_RECEIVE_TITLE :
                          (isVisitPurpose() ? UiStrings::VISIT_TITLE : UiStrings::LOBBY_SELECT_ROOM),
                          items, roomCount, selectedRoomIdx);
}

void LobbyScene::drawJoining() {
    PixelRenderer::fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::BLACK);
    PixelRenderer::drawPixelText(70, 50, UiStrings::LOBBY_JOINING, PixelRenderer::WHITE, 2);
    uint32_t elapsed = Hal::ins().millis() - stateStartMs;
    int dots = (elapsed / 500) % 4;
    char buf[12];
    snprintf(buf, sizeof(buf), "%.*s", dots, UiStrings::ELLIPSIS);
    PixelRenderer::drawPixelText(110, 80, buf, PixelRenderer::WHITE, 1);
    PixelRenderer::drawPixelText(6, 122, UiStrings::EXPLORE_CANCEL, PixelRenderer::GRAY, 1);
}

void LobbyScene::drawGiftWaiting() {
    PixelRenderer::fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::BLACK);
    PixelRenderer::drawPixelText(70, 50, UiStrings::GIFT_WAITING, PixelRenderer::WHITE, 2);
    uint32_t elapsed = Hal::ins().millis() - stateStartMs;
    int dots = (elapsed / 500) % 4;
    char buf[12];
    snprintf(buf, sizeof(buf), "%.*s", dots, UiStrings::ELLIPSIS);
    PixelRenderer::drawPixelText(110, 80, buf, PixelRenderer::WHITE, 1);
    PixelRenderer::drawPixelText(6, 122, UiStrings::EXPLORE_CANCEL, PixelRenderer::GRAY, 1);
}

void LobbyScene::drawGiftSending() {
    PixelRenderer::fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::BLACK);
    PixelRenderer::drawPixelText(75, 50, UiStrings::GIFT_SENDING, PixelRenderer::WHITE, 2);
    uint32_t elapsed = Hal::ins().millis() - stateStartMs;
    int dots = (elapsed / 500) % 4;
    char buf[12];
    snprintf(buf, sizeof(buf), "%.*s", dots, UiStrings::ELLIPSIS);
    PixelRenderer::drawPixelText(110, 80, buf, PixelRenderer::WHITE, 1);
    if (!giftSendStarted && !BattleLink::ins().isSending()) {
        PixelRenderer::drawPixelText(6, 122, UiStrings::EXPLORE_CANCEL, PixelRenderer::GRAY, 1);
    }
}

void LobbyScene::drawVisitSyncing() {
    PixelRenderer::fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::BLACK);
    PixelRenderer::drawPixelText(75, 50, UiStrings::VISIT_WAITING, PixelRenderer::WHITE, 2);
    uint32_t elapsed = Hal::ins().millis() - stateStartMs;
    int dots = (elapsed / 500) % 4;
    char buf[12];
    snprintf(buf, sizeof(buf), "%.*s", dots, UiStrings::ELLIPSIS);
    PixelRenderer::drawPixelText(110, 80, buf, PixelRenderer::WHITE, 1);
}

void LobbyScene::drawMessage() {
    PixelRenderer::fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::BLACK);
    if (messageText) {
        PixelRenderer::drawPixelText(50, 60, messageText, PixelRenderer::YELLOW, 1);
    }
}
