void MenuScene::enterMode(Mode nextMode) {
    Mode fromMode = mode;
    lastSelectedByMode[modeIndex(mode)] = selected;

    mode = nextMode;
    selected = lastSelectedByMode[modeIndex(mode)];
    if (mode == Mode::FOOD) {
        selected = GameEngine::ins().getFoodStyle();
        if (selected >= FOOD_ITEM_COUNT - 1) selected = 0;
        foodScroll = 0.0f;       // 让列表在首帧自然滚动到选中项
        foodConfirmTime = 0;     // 清除上一次确认反馈
    }
    else if (mode == Mode::GIFT_FOOD) {
        foodScroll = 0.0f;
        foodConfirmTime = 0;
    }
    else if (mode == Mode::TOY) {
        selected = GameEngine::ins().getToyStyle();
        if (selected >= TOY_ITEM_COUNT - 1) selected = TOY_NONE;
    }
    else if (mode == Mode::BOWL) {
        selected = GameEngine::ins().getBowlStyle();
        if (selected >= BOWL_ITEM_COUNT - 1) selected = 0;
        bowlScroll = 0.0f;
    }
    else if (mode == Mode::WOOD) {
        selected = GameEngine::ins().getWoodStyle();
        if (selected >= WOOD_ITEM_COUNT - 2) selected = 0; // 排除 Place 和 Back
        woodScroll = 0.0f;
    }
    else if (mode == Mode::EXPLORE) {
        if (selected >= EXPLORE_ITEM_COUNT - 1) selected = 0;
    }
    else if (mode == Mode::DEBUG) {
        if (selected >= DEBUG_ITEM_COUNT - 1) selected = 0;
    }
    else if (mode == Mode::DEBUG_STATE) {
        debugStageIndex = (int)GameEngine::ins().getBug().getStage();
        if (debugStageIndex < 0) debugStageIndex = 0;
        if (debugStageIndex > 4) debugStageIndex = 4;
        debugTemperIndex = (int)GameEngine::ins().getBug().getTemperament();
        if (debugTemperIndex < 0) debugTemperIndex = 0;
        if (debugTemperIndex > 5) debugTemperIndex = 5;
    }

    if (shouldStartSubmenuAtFirst(fromMode, nextMode)) {
        selected = 0;
    }

    int count = itemCount();
    if (selected >= count) selected = 0;
    animSelected = (float)selected;
}

bool MenuScene::shouldStartSubmenuAtFirst(Mode fromMode, Mode toMode) const {
    switch (fromMode) {
        case Mode::MAIN:
            return toMode == Mode::BOX ||
                   toMode == Mode::SOCIAL ||
                   toMode == Mode::EXPLORE ||
                   toMode == Mode::DEBUG;
        case Mode::BOX:
            return toMode == Mode::FOOD ||
                   toMode == Mode::WOOD ||
                   toMode == Mode::BOWL ||
                   toMode == Mode::TOY;
        case Mode::SOCIAL:
            return toMode == Mode::GIFT ||
                   toMode == Mode::FIGHT ||
                   toMode == Mode::VISIT;
        case Mode::GIFT:
            return toMode == Mode::GIFT_FOOD;
        case Mode::DEBUG:
            return toMode == Mode::DEBUG_STATE ||
                   toMode == Mode::DEBUG_ATTR;
        default:
            return false;
    }
}

int MenuScene::itemCount() const {
    if (mode == Mode::FOOD) return FOOD_ITEM_COUNT;
    if (mode == Mode::GIFT_FOOD) return giftFoodCount() + 1;
    if (mode == Mode::TOY) return TOY_ITEM_COUNT;
    if (mode == Mode::BOWL) return BOWL_ITEM_COUNT;
    if (mode == Mode::WOOD) return WOOD_ITEM_COUNT;
    if (mode == Mode::BOX) return BOX_ITEM_COUNT;
    if (mode == Mode::SOCIAL) return SOCIAL_ITEM_COUNT;
    if (mode == Mode::GIFT) return GIFT_ITEM_COUNT;
    if (mode == Mode::FIGHT) return FIGHT_ITEM_COUNT;
    if (mode == Mode::VISIT) return VISIT_ITEM_COUNT;
    if (mode == Mode::EXPLORE) return EXPLORE_ITEM_COUNT;
    if (mode == Mode::DEBUG) return DEBUG_ITEM_COUNT;
    if (mode == Mode::DEBUG_STATE) return DEBUG_STATE_ITEM_COUNT;
    if (mode == Mode::DEBUG_ATTR) return DEBUG_ATTR_ITEM_COUNT;
    return MAIN_ITEM_COUNT;
}

const char* MenuScene::itemLabel(int index, char* buf, size_t bufSize) const {
    if (mode == Mode::FOOD || mode == Mode::GIFT_FOOD) {
        if (!isVisibleFoodBackIndex(index)) {
            return FoodTypeInfo::name(visibleFoodAt(index));
        }
        return UiStrings::BACK;
    }

    if (mode == Mode::BOWL) {
        if (index >= 0 && index < BOWL_ITEM_COUNT - 1) {
            return BowlAssets::NAME[index];
        }
        return UiStrings::BACK;
    }

    if (mode == Mode::TOY) {
        switch (index) {
            case TOY_NONE: return UiStrings::TOY_NONE;
            case TOY_BALL: return UiStrings::TOY_BALL;
            case TOY_BACK:
            default: return UiStrings::BACK;
        }
    }

    if (mode == Mode::WOOD) {
        if (index == WOOD_NONE) {
            return UiStrings::WOOD_NONE;
        }
        if (index >= 0 && index < WOOD_ITEM_COUNT - 1) {
            return WoodAssets::NAME[index - 1];
        }
        return UiStrings::BACK;
    }

    if (mode == Mode::BOX) {
        switch (index) {
            case BOX_FOOD:
                return UiStrings::MENU_FEED;
            case BOX_WOOD:
                return UiStrings::MENU_WOOD;
            case BOX_BOWL:
                return UiStrings::MENU_BOWL;
            case BOX_BG:
                snprintf(buf, bufSize, "%s:%s", UiStrings::BG,
                         GameEngine::ins().getMainSceneBgName());
                return buf;
            case BOX_TOY:
                return UiStrings::MENU_TOY;
            case BOX_SLEEP:
                return UiStrings::MENU_SLEEP;
            case BOX_BACK:
            default:
                return UiStrings::BACK;
        }
    }

    if (mode == Mode::SOCIAL) {
        switch (index) {
            case SOCIAL_GIFT: return UiStrings::MENU_SOCIAL_GIFT;
            case SOCIAL_FIGHT: return UiStrings::MENU_SOCIAL_FIGHT;
            case SOCIAL_VISIT: return UiStrings::MENU_SOCIAL_VISIT;
            case SOCIAL_BACK:
            default: return UiStrings::BACK;
        }
    }

    if (mode == Mode::GIFT) {
        switch (index) {
            case GIFT_FOOD: return UiStrings::MENU_GIFT_FOOD;
            case GIFT_TOY: return UiStrings::MENU_GIFT_TOY;
            case GIFT_RECEIVE: return UiStrings::MENU_GIFT_RECEIVE;
            case GIFT_BACK:
            default: return UiStrings::BACK;
        }
    }

    if (mode == Mode::FIGHT) {
        switch (index) {
            case FIGHT_CREATE: return UiStrings::MENU_FIGHT_CREATE;
            case FIGHT_SEARCH: return UiStrings::MENU_FIGHT_SEARCH;
            case FIGHT_BACK:
            default: return UiStrings::BACK;
        }
    }

    if (mode == Mode::VISIT) {
        switch (index) {
            case VISIT_CREATE: return UiStrings::MENU_FIGHT_CREATE;
            case VISIT_SEARCH: return UiStrings::MENU_FIGHT_SEARCH;
            case VISIT_BACK:
            default: return UiStrings::BACK;
        }
    }

    if (mode == Mode::EXPLORE) {
        switch (index) {
            case LOCATION_PARK: return UiStrings::LOCATION_PARK;
            case LOCATION_BACK_HILL: return UiStrings::LOCATION_BACK_HILL;
            case LOCATION_RIVERSIDE: return UiStrings::LOCATION_RIVERSIDE;
            case LOCATION_OLD_WOODS: return UiStrings::LOCATION_OLD_WOODS;
            case LOCATION_CUP: return UiStrings::MENU_FIGHT_CUP;
            case LOCATION_BACK:
            default: return UiStrings::BACK;
        }
    }

    if (mode == Mode::DEBUG) {
        switch (index) {
            case DEBUG_BEETLE: return UiStrings::MENU_DEBUG_BEETLE;
            case DEBUG_ATTR: return UiStrings::MENU_DEBUG_ATTR;
            case DEBUG_NPC: return UiStrings::MENU_DEBUG_NPC;
            case DEBUG_CLEAR_SUBSTRATE: return UiStrings::MENU_DEBUG_CLEAR_SUBSTRATE;
            case DEBUG_UNLOCK_ALL: return UiStrings::MENU_DEBUG_UNLOCK_ALL;
            case DEBUG_BACK:
            default: return UiStrings::BACK;
        }
    }

    if (mode == Mode::DEBUG_STATE) {
        switch (index) {
            case DEBUG_STATE_STAGE:
                snprintf(buf, bufSize, UiStrings::MENU_DEBUG_STATE_STAGE_FMT,
                         stageName((Stage)debugStageIndex));
                return buf;
            case DEBUG_STATE_TEMPER:
                snprintf(buf, bufSize, UiStrings::MENU_DEBUG_STATE_TEMPER_FMT,
                         temperamentName((Temperament)debugTemperIndex));
                return buf;
            case DEBUG_STATE_BACK:
            default:
                return UiStrings::BACK;
        }
    }

    if (mode == Mode::DEBUG_ATTR) {
        if (index >= DEBUG_ATTR_SIZ && index <= DEBUG_ATTR_SPI) {
            const Bug& bug = GameEngine::ins().getBug();
            snprintf(buf, bufSize, UiStrings::MENU_DEBUG_ATTR_FMT, attrName((uint8_t)index),
                     bug.debugGetAttr((uint8_t)index),
                     bug.debugGetAttrCap((uint8_t)index));
            return buf;
        }
        return UiStrings::BACK;
    }

    switch (index) {
        case INFO: return UiStrings::MENU_INFO;
        case BOX: return UiStrings::MENU_BOX;
        case SOCIAL: return UiStrings::MENU_SOCIAL;
        case EXPLORE: return UiStrings::MENU_EXPLORE;
        case SETTINGS: return UiStrings::MENU_SETTINGS;
        case DEBUG: return UiStrings::MENU_DEBUG;
        case BACK:
        default:
            return UiStrings::BACK;
    }
}

void MenuScene::saveSettingsNow() {
    SaveManager::ins().saveSettings(
        PixelRenderer::getContentFontScale(),
        Hal::ins().getBrightness(),
        GameEngine::ins().getGameSpeed(),
        GameEngine::ins().getIdleTimeoutIndex(),
        GameEngine::ins().getMainSceneBg(),
        GameEngine::ins().getWoodStyle(),
        GameEngine::ins().getBowlStyle(),
        GameEngine::ins().getFoodStyle(),
        GameEngine::ins().getToyStyle()
    );
}

void MenuScene::executeSleep() {
    sleepConfirmActive = false;
    if (!GameEngine::ins().sleepUntilMorning()) {
        if (GameEngine::ins().getBug().isDead()) {
            showToast(UiStrings::SLEEP_BEETLE_DIED);
        } else {
            showToast(UiStrings::SLEEP_TOO_EARLY);
        }
        Serial.println("[Menu] Sleep until morning rejected");
        return;
    }
    startSleepTransition();
    Serial.println("[Menu] Sleep until morning executed");
}

bool MenuScene::isCupAvailable() const {
    const Bug& bug = GameEngine::ins().getBug();
    if (!GameEngine::ins().isFeatureUnlocked(GameEngine::FEATURE_CUP_UNLOCKED)) return false;
    if (bug.getStage() != Stage::ADULT || bug.isDead() || bug.getHunger() < 30) return false;
    return GameEngine::ins().getCupCycleState() == GameEngine::CupCycleState::REGISTER_OPEN;
}

const char* MenuScene::cupClosedMessage() {
    uint64_t now = GameEngine::ins().getGameNow();
    uint64_t cycleStart = (uint64_t)GameEngine::ins().getLastCupGameTime() * 1000ULL;
    uint64_t target = cycleStart;

    GameEngine::CupCycleState state = GameEngine::ins().getCupCycleState();
    if (state == GameEngine::CupCycleState::REGISTER_EXPIRED ||
        state == GameEngine::CupCycleState::IN_PROGRESS ||
        now >= cycleStart + GameEngine::CUP_REGISTER_MS) {
        target = cycleStart + GameEngine::CUP_CYCLE_MS;
    }
    while (target <= now) {
        target += GameEngine::CUP_CYCLE_MS;
    }

    uint64_t remaining = target - now;
    if (remaining >= GameEngine::GAME_DAY_MS) {
        uint32_t days = (uint32_t)((remaining + GameEngine::GAME_DAY_MS - 1) /
                                   GameEngine::GAME_DAY_MS);
        snprintf(cupClosedToast, sizeof(cupClosedToast),
                 UiStrings::CUP_NEXT_DAY_FMT, days,
                 days == 1 ? "" : UiStrings::PLURAL_S);
        return cupClosedToast;
    }

    static constexpr uint64_t HOUR_MS = 60ULL * 60 * 1000;
    uint32_t hours = (uint32_t)((remaining + HOUR_MS - 1) / HOUR_MS);
    if (hours > 0) {
        snprintf(cupClosedToast, sizeof(cupClosedToast),
                 UiStrings::CUP_NEXT_HOUR_FMT, hours,
                 hours == 1 ? "" : UiStrings::PLURAL_S);
        return cupClosedToast;
    }

    snprintf(cupClosedToast, sizeof(cupClosedToast), "%s", UiStrings::CUP_NEXT_SOON);
    return cupClosedToast;
}

const char* MenuScene::stageName(Stage stage) const {
    switch (stage) {
        case Stage::EGG: return UiStrings::STAGE_EGG;
        case Stage::LARVA: return UiStrings::STAGE_LARVA;
        case Stage::PUPA: return UiStrings::STAGE_PUPA;
        case Stage::JUVENILE: return UiStrings::STAGE_JUVENILE;
        case Stage::ADULT: return UiStrings::STAGE_ADULT;
        default: return UiStrings::UNKNOWN_SHORT;
    }
}

const char* MenuScene::temperamentName(Temperament temperament) const {
    switch (temperament) {
        case Temperament::SWIFT: return UiStrings::TEMP_SWIFT;
        case Temperament::RESILIENT: return UiStrings::TEMP_RESILIENT;
        case Temperament::GIANT: return UiStrings::TEMP_GIANT;
        case Temperament::BRUTE: return UiStrings::TEMP_BRUTE;
        case Temperament::BALANCED: return UiStrings::TEMP_BALANCED;
        case Temperament::SPIRIT: return UiStrings::TEMP_SPIRIT;
        default: return UiStrings::UNKNOWN_SHORT;
    }
}

const char* MenuScene::attrName(uint8_t index) const {
    switch (index) {
        case 0: return UiStrings::ATTR_SIZ;
        case 1: return UiStrings::ATTR_STR;
        case 2: return UiStrings::ATTR_END;
        case 3: return UiStrings::ATTR_SPD;
        case 4: return UiStrings::ATTR_SPI;
        default: return UiStrings::UNKNOWN_SHORT;
    }
}

void MenuScene::openAttrEdit(uint8_t index) {
    attrEditIndex = index;
    attrEditValue = roundf(GameEngine::ins().getBug().debugGetAttr(index));
    clampAttrEditValue();
    attrEditButton = ATTR_EDIT_INC;
    attrEditActive = true;
}

void MenuScene::clampAttrEditValue() {
    if (attrEditValue < 1.0f) attrEditValue = 1.0f;
    uint8_t cap = GameEngine::ins().getBug().debugGetAttrCap(attrEditIndex);
    if (attrEditValue > cap) attrEditValue = (float)cap;
}

bool MenuScene::handleAttrEditButton(const ButtonEvent& ev) {
    if (ev.action == BtnAction::LONG_PRESS && ev.btn == 1) {
        attrEditActive = false;
        return true;
    }
    if (ev.action != BtnAction::PRESSED) return true;

    if (ev.btn == 1) {
        attrEditButton++;
        if (attrEditButton > ATTR_EDIT_YES) attrEditButton = ATTR_EDIT_DEC;
        return true;
    }

    if (ev.btn == 0) {
        if (attrEditButton == ATTR_EDIT_DEC) {
            attrEditValue -= 1.0f;
            clampAttrEditValue();
        } else if (attrEditButton == ATTR_EDIT_INC) {
            attrEditValue += 1.0f;
            clampAttrEditValue();
        } else if (attrEditButton == ATTR_EDIT_YES) {
            GameEngine::ins().getBug().debugSetAttr(attrEditIndex, attrEditValue);
            GameEngine::ins().forceSave();
            attrEditActive = false;
            showToast(UiStrings::SAVED);
            Serial.printf("[Menu] Debug attr %s set to %.0f\n", attrName(attrEditIndex), attrEditValue);
        }
        return true;
    }

    return true;
}

int MenuScene::giftFoodCount() const {
    const Bug& bug = GameEngine::ins().getBug();
    int count = 0;
    for (uint8_t i = 0; i < (uint8_t)FoodType::COUNT; ++i) {
        if (bug.getFoodCount((FoodType)i) > 0) ++count;
    }
    return count;
}

FoodType MenuScene::giftFoodAt(int index) const {
    const Bug& bug = GameEngine::ins().getBug();
    int seen = 0;
    for (uint8_t i = 0; i < (uint8_t)FoodType::COUNT; ++i) {
        FoodType type = (FoodType)i;
        if (bug.getFoodCount(type) == 0) continue;
        if (seen == index) return type;
        ++seen;
    }
    return FoodType::DROP;
}

bool MenuScene::isGiftFoodBackIndex(int index) const {
    return index == giftFoodCount();
}

FoodType MenuScene::visibleFoodAt(int index) const {
    if (mode == Mode::GIFT_FOOD) return giftFoodAt(index);
    if (index < 0 || index >= (int)FoodType::COUNT) return FoodType::DROP;
    return (FoodType)index;
}

bool MenuScene::isVisibleFoodBackIndex(int index) const {
    if (mode == Mode::GIFT_FOOD) return isGiftFoodBackIndex(index);
    return index == FOOD_BACK;
}

void MenuScene::openGiftQuantity(ItemId id, uint8_t maxAmount) {
    giftQuantityItemId = id;
    giftQuantityMax = maxAmount;
    giftQuantityValue = 0;
    giftQuantityButton = GIFT_QTY_INC;
    clampGiftQuantityValue();
    giftQuantityActive = true;
}

void MenuScene::clampGiftQuantityValue() {
    if (giftQuantityValue > giftQuantityMax) giftQuantityValue = giftQuantityMax;
}

bool MenuScene::handleGiftQuantityButton(const ButtonEvent& ev) {
    if (ev.action == BtnAction::LONG_PRESS && ev.btn == 1) {
        giftQuantityActive = false;
        return true;
    }
    if (ev.action != BtnAction::PRESSED) return true;

    if (ev.btn == 1) {
        giftQuantityButton++;
        if (giftQuantityButton > GIFT_QTY_YES) giftQuantityButton = GIFT_QTY_DEC;
        return true;
    }

    if (ev.btn == 0) {
        if (giftQuantityButton == GIFT_QTY_DEC) {
            if (giftQuantityValue > 0) --giftQuantityValue;
        } else if (giftQuantityButton == GIFT_QTY_INC) {
            if (giftQuantityValue < giftQuantityMax) ++giftQuantityValue;
        } else if (giftQuantityButton == GIFT_QTY_YES) {
            clampGiftQuantityValue();
            if (giftQuantityValue == 0) {
                showToast(UiStrings::GIFT_AMOUNT_ZERO);
                return true;
            }
            GameEngine::ins().setPendingGiftItem(giftQuantityItemId, giftQuantityValue);
            GameEngine::ins().setLobbyMode(LobbyMode::LOBBY_GIFT_SEND);
            giftQuantityActive = false;
            nextScene = SCENE_LOBBY;
            Serial.printf("[Menu] Gift selected: %s x%u\n",
                          ItemCatalog::name(giftQuantityItemId), giftQuantityValue);
        }
        return true;
    }

    return true;
}

void MenuScene::showToast(const char* msg, uint32_t durationMs) {
    if (!msg || msg[0] == '\0') {
        toastText[0] = '\0';
        toastMsg = nullptr;
        toastEndMs = 0;
        return;
    }
    snprintf(toastText, sizeof(toastText), "%s", msg);
    toastMsg = toastText;
    toastEndMs = Hal::ins().millis() + durationMs;
}

void MenuScene::drawToast() {
    if (!toastMsg) return;
    uint64_t now = Hal::ins().millis();
    if (now >= toastEndMs) {
        toastMsg = nullptr;
        return;
    }

    float fs = PixelRenderer::getContentFontScale();
    LGFX_Sprite& canvas = Hal::ins().canvas();

    // 计算行数与最宽行
    int lineCount = 1;
    int maxLineW = 0;
    const char* p = toastMsg;
    const char* lineStart = toastMsg;
    while (true) {
        if (*p == '\n' || *p == '\0') {
            if (*p == '\n') lineCount++;
            char buf[64];
            int len = min((int)(p - lineStart), (int)sizeof(buf) - 1);
            memcpy(buf, lineStart, len);
            buf[len] = '\0';
            PixelRenderer::applyTextStyle(fs);
            int w = canvas.textWidth(buf);
            if (w > maxLineW) maxLineW = w;
            if (*p == '\0') break;
            lineStart = p + 1;
        }
        p++;
    }

    int padX = (int)(8 * fs);
    int padY = (int)(6 * fs);
    int lineH = (int)(12 * fs);
    int boxW = maxLineW + padX * 2;
    int boxH = lineCount * lineH + padY * 2;
    int boxX = (Hal::DISPLAY_W - boxW) / 2;
    int boxY = (Hal::DISPLAY_H - boxH) / 2;

    PixelRenderer::fillRect(boxX, boxY, boxW, boxH, PixelRenderer::rgb565(0, 0, 0));
    canvas.drawRect(boxX, boxY, boxW, boxH, PixelRenderer::WHITE);

    int y = boxY + padY;
    p = toastMsg;
    lineStart = toastMsg;
    while (true) {
        if (*p == '\n' || *p == '\0') {
            char buf[64];
            int len = min((int)(p - lineStart), (int)sizeof(buf) - 1);
            memcpy(buf, lineStart, len);
            buf[len] = '\0';
            PixelRenderer::applyTextStyle(fs);
            int w = canvas.textWidth(buf);
            int x = boxX + (boxW - w) / 2;
            PixelRenderer::drawPixelText(x, y, buf, PixelRenderer::WHITE, fs);
            y += lineH;
            if (*p == '\0') break;
            lineStart = p + 1;
        }
        p++;
    }
}
