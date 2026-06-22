#include "LobbyScene.h"
#include "../core/GameEngine.h"
#include "../core/UiStrings.h"
#include "../hardware/Hal.h"
#include "../hardware/PixelRenderer.h"

void LobbyScene::onEnter() {
    state = State::MODE_SELECT;
    selectedMode = Mode::NONE;
    roomId = 0;
    stateStartMs = Hal::ins().millis();
    lastScanLogMs = 0;
    selectedRoomIdx = 0;
    roomCount = 0;
    messageText = nullptr;
    BattleLink::ins().begin();

    // 由 Fight 子菜单指定入口模式时直接跳转
    LobbyMode startMode = GameEngine::ins().getLobbyMode();
    GameEngine::ins().setLobbyMode(LobbyMode::LOBBY_DEFAULT);
    switch (startMode) {
        case LobbyMode::LOBBY_CREATE:
            enterHostWaiting();
            break;
        case LobbyMode::LOBBY_SEARCH:
            enterSearchScanning();
            break;
        default:
            enterModeSelect();
            break;
    }

    Serial.println("[Lobby] entered");
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
                // 接受加入，设置对端为主机身份
                BattleLink::ins().setBattlePeer(req.mac, true);
                BattleLink::ins().sendJoinAck(true, req.mac);
                Serial.printf("[Lobby] host accepted joiner %02X:%02X, goto battle\n",
                              req.mac[0], req.mac[5]);
                nextScene = SCENE_BATTLE;
                return nextScene;
            }
            if (now - stateStartMs >= HOST_TIMEOUT_MS) {
                Serial.println("[Lobby] host timeout");
                enterMessage(UiStrings::MSG_NO_ONE_JOINED);
            }
            break;
        }

        case State::SEARCH_SCANNING: {
            if (now - lastScanLogMs >= 1000) {
                uint8_t count = BattleLink::ins().getRoomListCount();
                Serial.printf("[Lobby] scanning... found %d rooms\n", count);
                lastScanLogMs = now;
            }
            if (now - stateStartMs >= SEARCH_SCAN_MS) {
                roomCount = BattleLink::ins().getRoomListCount();
                if (roomCount == 0) {
                    enterMessage(UiStrings::LOBBY_NO_ROOMS);
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
                if (ack.accepted) {
                    uint8_t hostMac[6];
                    memcpy(hostMac, ack.host_mac, 6);
                    BattleLink::ins().setBattlePeer(hostMac, false);
                    Serial.printf("[Lobby] joined room, host=%02X:%02X, goto battle\n",
                                  hostMac[0], hostMac[5]);
                    nextScene = SCENE_BATTLE;
                    return nextScene;
                } else {
                    Serial.println("[Lobby] join rejected");
                    enterMessage(UiStrings::MSG_REJECTED);
                }
                break;
            }
            if (now - stateStartMs >= JOIN_TIMEOUT_MS) {
                Serial.println("[Lobby] join timeout");
                enterMessage(UiStrings::MSG_JOIN_TIMEOUT);
            }
            break;
        }

        case State::MESSAGE: {
            if (now - stateStartMs >= MESSAGE_MS) {
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
            enterModeSelect(Mode::SEARCH);
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

void LobbyScene::enterHostWaiting() {
    state = State::HOST_WAITING;
    roomId = (uint8_t)random(1, 256);
    BattleLink::ins().startRoomHost(roomId);
    stateStartMs = Hal::ins().millis();
    Serial.printf("[Lobby] hosting room %d\n", roomId);
}

void LobbyScene::enterSearchScanning() {
    state = State::SEARCH_SCANNING;
    BattleLink::ins().startRoomSearch();
    stateStartMs = Hal::ins().millis();
    lastScanLogMs = 0;
    Serial.println("[Lobby] start scanning");
}

void LobbyScene::enterSearchList() {
    state = State::SEARCH_LIST;
    roomCount = BattleLink::ins().getRoomListCount();
    if (selectedRoomIdx >= roomCount) selectedRoomIdx = 0;
    Serial.printf("[Lobby] show %d rooms\n", roomCount);
}

void LobbyScene::enterJoining(uint8_t idx) {
    const BattleLink::RoomEntry* entry = BattleLink::ins().getRoomListEntry(idx);
    if (!entry) {
        enterSearchScanning();
        return;
    }
    state = State::JOINING;
    BattleLink::ins().sendJoinReq(entry->room_id, entry->mac);
    stateStartMs = Hal::ins().millis();
    Serial.printf("[Lobby] joining room %d idx=%d\n", entry->room_id, idx);
}

void LobbyScene::enterMessage(const char* text) {
    state = State::MESSAGE;
    messageText = text;
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
    const char* items[3] = { "Create Room", "Search Room", "Back" };
    uint8_t cursor = 0;
    if (selectedMode == Mode::SEARCH) cursor = 1;
    else if (selectedMode == Mode::BACK) cursor = 2;
    drawSettingsStyleList(UiStrings::MENU_FIGHT, items, 3, cursor);
}

void LobbyScene::drawHostWaiting() {
    PixelRenderer::fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::BLACK);
    PixelRenderer::drawPixelText(80, 25, UiStrings::LOBBY_ROOM, PixelRenderer::WHITE, 2);

    char buf[16];
    snprintf(buf, sizeof(buf), "%03d", roomId);
    PixelRenderer::drawPixelText(90, 55, buf, PixelRenderer::YELLOW, 3);

    uint32_t elapsed = Hal::ins().millis() - stateStartMs;
    int dots = (elapsed / 500) % 4;
    snprintf(buf, sizeof(buf), "%s%.*s", UiStrings::LOBBY_WAITING, dots, "...");
    PixelRenderer::drawPixelText(75, 100, buf, PixelRenderer::WHITE, 1);
}

void LobbyScene::drawSearchScanning() {
    PixelRenderer::fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::BLACK);
    PixelRenderer::drawPixelText(70, 50, UiStrings::LOBBY_SCANNING, PixelRenderer::WHITE, 2);
    uint32_t elapsed = Hal::ins().millis() - stateStartMs;
    int dots = (elapsed / 500) % 4;
    char buf[12];
    snprintf(buf, sizeof(buf), "%.*s", dots, "...");
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
    drawSettingsStyleList(UiStrings::LOBBY_SELECT_ROOM, items, roomCount, selectedRoomIdx);
}

void LobbyScene::drawJoining() {
    PixelRenderer::fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::BLACK);
    PixelRenderer::drawPixelText(70, 50, UiStrings::LOBBY_JOINING, PixelRenderer::WHITE, 2);
    uint32_t elapsed = Hal::ins().millis() - stateStartMs;
    int dots = (elapsed / 500) % 4;
    char buf[12];
    snprintf(buf, sizeof(buf), "%.*s", dots, "...");
    PixelRenderer::drawPixelText(110, 80, buf, PixelRenderer::WHITE, 1);
}

void LobbyScene::drawMessage() {
    PixelRenderer::fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::BLACK);
    if (messageText) {
        PixelRenderer::drawPixelText(50, 60, messageText, PixelRenderer::YELLOW, 1);
    }
}
