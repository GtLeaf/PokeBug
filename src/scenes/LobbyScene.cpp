#include "LobbyScene.h"
#include "../core/GameEngine.h"
#include "../core/UiStrings.h"
#include "../hardware/Hal.h"
#include "../hardware/PixelRenderer.h"
#include "../game/ItemCatalog.h"
#include <cmath>
#include <cstring>

namespace {

uint8_t visitPaletteCode(Temperament temperament, float depth) {
    uint8_t t = (uint8_t)temperament;
    if (t > (uint8_t)Temperament::SPIRIT) t = (uint8_t)Temperament::SPIRIT;
    if (depth < 0.0f) depth = 0.0f;
    if (depth > 1.0f) depth = 1.0f;
    uint8_t bucket = (uint8_t)(depth * 4.0f);
    if (bucket > 3) bucket = 3;
    return (uint8_t)(0x80 | (bucket << 3) | t);
}

}

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
            if (now - stateStartMs >= JOIN_TIMEOUT_MS) {
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
                if (ItemCatalog::kind(gift.item_id) != ItemKind::FOOD) {
                    enterMessage(UiStrings::GIFT_UNSUPPORTED, true);
                } else if (GameEngine::ins().getBug().addItem(gift.item_id, gift.amount)) {
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
                if (ok && GameEngine::ins().getBug().removeItem(gift.id, gift.amount)) {
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
    } else if (state == State::SEARCH_SCANNING) {
        // 搜索过程中按 B 取消，回到模式选择（默认选中 SEARCH）
        if (ev.btn == 1) {
            if (isDirectMenuPurpose()) nextScene = SCENE_MENU;
            else enterModeSelect(Mode::SEARCH);
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
    if (ItemCatalog::kind(gift.id) != ItemKind::FOOD) return false;
    return GameEngine::ins().getBug().getItemCount(gift.id) >= gift.amount;
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
}

void LobbyScene::drawGiftWaiting() {
    PixelRenderer::fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::BLACK);
    PixelRenderer::drawPixelText(70, 50, UiStrings::GIFT_WAITING, PixelRenderer::WHITE, 2);
    uint32_t elapsed = Hal::ins().millis() - stateStartMs;
    int dots = (elapsed / 500) % 4;
    char buf[12];
    snprintf(buf, sizeof(buf), "%.*s", dots, UiStrings::ELLIPSIS);
    PixelRenderer::drawPixelText(110, 80, buf, PixelRenderer::WHITE, 1);
}

void LobbyScene::drawGiftSending() {
    PixelRenderer::fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::BLACK);
    PixelRenderer::drawPixelText(75, 50, UiStrings::GIFT_SENDING, PixelRenderer::WHITE, 2);
    uint32_t elapsed = Hal::ins().millis() - stateStartMs;
    int dots = (elapsed / 500) % 4;
    char buf[12];
    snprintf(buf, sizeof(buf), "%.*s", dots, UiStrings::ELLIPSIS);
    PixelRenderer::drawPixelText(110, 80, buf, PixelRenderer::WHITE, 1);
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
