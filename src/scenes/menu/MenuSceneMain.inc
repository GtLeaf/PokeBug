void MenuScene::onEnter() {
    mode = GameEngine::ins().getPrevSceneID() == SCENE_LOBBY ? Mode::SOCIAL : Mode::MAIN;
    Bug& bug = GameEngine::ins().getBug();
    if (careItemsNotNeeded(bug.getStage())) {
        bool changed = bug.hasFoodInTray() || bug.isWoodPlaced();
        bug.setFoodTray(0, (FoodType)GameEngine::ins().getFoodStyle());
        bug.removeWood();
        if (changed) GameEngine::ins().forceSave();
    }

    // 从培养缸进入时重置；从 Lobby 回来时回到 social；从子菜单返回时保持上次位置。
    if (GameEngine::ins().getPrevSceneID() == SCENE_TERRARIUM) {
        selected = 0;
    } else {
        selected = lastSelectedByMode[modeIndex(mode)];
    }
    animSelected = (float)selected;

    // Debug 阶段切换索引初始化为当前甲虫阶段
    debugStageIndex = (int)bug.getStage();
    if (debugStageIndex < 0) debugStageIndex = 0;
    if (debugStageIndex > 4) debugStageIndex = 4;
    debugTemperIndex = (int)bug.getTemperament();
    if (debugTemperIndex < 0) debugTemperIndex = 0;
    if (debugTemperIndex > 5) debugTemperIndex = 5;
    attrEditActive = false;
    giftQuantityActive = false;
    lastVisitStatusMs = 0;
}

void MenuScene::onExit() {
    if (sleepTransitionActive) {
        Hal::ins().setBrightness(sleepTransitionBaseBrightness);
        sleepTransitionActive = false;
    }

    lastSelectedByMode[modeIndex(mode)] = selected;
}


SceneID MenuScene::update() {
    uint32_t nowMs = Hal::ins().millis();
    serviceVisitLink(nowMs);

    if (sleepTransitionActive) {
        uint32_t elapsed = nowMs - sleepTransitionStartMs;
        if (elapsed >= SLEEP_TRANSITION_MS) {
            Hal::ins().setBrightness(sleepTransitionBaseBrightness);
            sleepTransitionActive = false;
            return SCENE_TERRARIUM;
        }
        Hal::ins().setBrightness(sleepTransitionBrightness(elapsed));
    }
    return nextScene;
}

void MenuScene::serviceVisitLink(uint32_t nowMs) {
    GameEngine& engine = GameEngine::ins();
    if (!engine.hasActiveVisitSession()) {
        lastVisitStatusMs = 0;
        return;
    }
    if (!engine.isVisitHost()) return;

    BattleLink& link = BattleLink::ins();
    if (link.isBattlePeerSet()) {
        link.update();
        uint8_t remoteHunger = 0;
        uint8_t remoteMotivation = 0;
        while (link.takeReceivedVisitVitals(remoteHunger, remoteMotivation)) {
            engine.setVisitRemoteVitals(remoteHunger, remoteMotivation);
        }
        if (link.takeReceivedVisitRecall()) {
            engine.clearVisitSession();
            lastVisitStatusMs = 0;
            Serial.println("[Menu] Visit recalled by peer");
            return;
        }
    }

    if (!link.isBattlePeerSet() || link.isSending()) return;
    if (lastVisitStatusMs != 0 &&
        nowMs - lastVisitStatusMs < MENU_VISIT_STATUS_INTERVAL_MS) {
        return;
    }
    if (link.sendVisitStatus(engine.getVisitRemainingMs(),
                             engine.getVisitDurationMs(),
                             engine.getGameSpeedX10(),
                             true,
                             engine.getVisitRemoteEatCount(),
                             engine.getVisitRemotePlayCount())) {
        lastVisitStatusMs = nowMs;
    }
}

void MenuScene::render() {
    PixelRenderer::fillRect(0, 0, 240, 135, PixelRenderer::rgb565(0, 0, 0));

    if (sleepTransitionActive) {
        drawSleepTransition();
        return;
    }

    if (mode == Mode::MAIN) {
        drawBattery();
    }
    if (sleepConfirmActive) {
        drawSleepConfirm();
        drawToast();
        return;
    }

    if (mode == Mode::FOOD || mode == Mode::GIFT_FOOD) {
        drawFoodLayout();
    } else if (mode == Mode::WOOD) {
        drawWoodLayout();
    } else if (mode == Mode::BOWL) {
        drawBowlLayout();
    } else if (mode == Mode::TOY || mode == Mode::SOCIAL || mode == Mode::GIFT || mode == Mode::FIGHT ||
               mode == Mode::DEBUG_STATE || mode == Mode::DEBUG_ATTR) {
        drawSimpleList();
    } else if (mode == Mode::EXPLORE) {
        drawExploreList();
    } else if (mode == Mode::DEBUG) {
        drawDebugList();
    } else {
        drawList();
    }

    if (attrEditActive) {
        drawAttrEditDialog();
    }
    if (giftQuantityActive) {
        drawGiftQuantityDialog();
    }

    drawToast();
}

void MenuScene::drawBattery() {
    float fs = PixelRenderer::getContentFontScale();
    int level = Hal::ins().batteryLevel();
    char buf[16];
    if (level < 0) {
        snprintf(buf, sizeof(buf), "--%%");
    } else {
        if (level > 100) level = 100;
        snprintf(buf, sizeof(buf), "%d%%", level);
    }
    uint16_t color = (level < 20) ? PixelRenderer::RED : PixelRenderer::GREEN;

    LGFX_Sprite& canvas = Hal::ins().canvas();
    PixelRenderer::applyTextStyle(fs);
    int w = canvas.textWidth(buf);
    int x = Hal::DISPLAY_W - w - (int)(6 * fs);
    PixelRenderer::drawPixelText(x, 6, buf, color, fs);
}

void MenuScene::drawList() {
    static constexpr int CENTER_Y = Hal::DISPLAY_H / 2;
    static constexpr int SPACING = 42;
    static constexpr float LERP = 0.25f;

    float fs = PixelRenderer::getContentFontScale();
    LGFX_Sprite& canvas = Hal::ins().canvas();

    // 让 animSelected 平滑追上 selected
    float target = (float)selected;
    float diff = target - animSelected;
    if (fabsf(diff) < 0.05f) {
        animSelected = target;
    } else {
        animSelected += diff * LERP;
    }

    // 按距离中心远近排序，确保选中项最后画
    int count = itemCount();
    int order[MAIN_ITEM_COUNT];
    for (int i = 0; i < count; i++) order[i] = i;
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            float di = fabsf((float)order[i] - animSelected);
            float dj = fabsf((float)order[j] - animSelected);
            if (dj < di) {
                int t = order[i]; order[i] = order[j]; order[j] = t;
            }
        }
    }

    static constexpr int BOX_W = 56;
    static constexpr int BOX_H = 32;
    static constexpr int ICON_SLOT_W = 56;
    int boxX = (int)(10 * fs) + 10;

    for (int k = 0; k < count; k++) {
        int i = order[k];
        float rawOffset = (float)i - animSelected;
        int y = CENTER_Y + (int)(rawOffset * SPACING);

        bool isSelected = (fabsf(rawOffset) < 0.5f);
        float relScale = isSelected ? 1.15f : 1.0f;
        uint16_t boxColor = isSelected ? PixelRenderer::YELLOW : PixelRenderer::GRAY;
        uint16_t descColor = isSelected ? PixelRenderer::WHITE : PixelRenderer::GRAY;

        int leftW = BOX_W;
        int iconIndex = i;

        if (mode == Mode::MAIN && iconIndex >= 0 && iconIndex < MenuAssets::MAIN_ICON_COUNT) {
            if (isSelected) {
                PixelRenderer::fillRect(boxX - 4, y - BOX_H / 2, 2, BOX_H, PixelRenderer::YELLOW);
            }

            uint16_t offset = pgm_read_word(&MenuAssets::MAIN_ICON_FRAMES[iconIndex].offset);
            uint16_t length = pgm_read_word(&MenuAssets::MAIN_ICON_FRAMES[iconIndex].length);
            float iconScale = relScale;
            int scaledIconW = (int)(MenuAssets::FRAME_W * iconScale);
            int scaledIconH = (int)(MenuAssets::FRAME_H * iconScale);
            int iconX = boxX + (ICON_SLOT_W - scaledIconW) / 2;
            int iconY = y - scaledIconH / 2;
            PixelRenderer::drawRgb565RleScaled(iconX, iconY, MenuAssets::FRAME_W, MenuAssets::FRAME_H,
                                               MenuAssets::MAIN_ICON_RLE, offset, length, iconScale);
            leftW = ICON_SLOT_W;
        } else if (mode == Mode::BOX && iconIndex >= 0 && iconIndex < MenuAssets::BOX_ICON_COUNT) {
            if (isSelected) {
                PixelRenderer::fillRect(boxX - 4, y - BOX_H / 2, 2, BOX_H, PixelRenderer::YELLOW);
            }

            uint16_t offset = pgm_read_word(&MenuAssets::BOX_ICON_FRAMES[iconIndex].offset);
            uint16_t length = pgm_read_word(&MenuAssets::BOX_ICON_FRAMES[iconIndex].length);
            float iconScale = relScale;
            int scaledIconW = (int)(MenuAssets::FRAME_W * iconScale);
            int scaledIconH = (int)(MenuAssets::FRAME_H * iconScale);
            int iconX = boxX + (ICON_SLOT_W - scaledIconW) / 2;
            int iconY = y - scaledIconH / 2;
            PixelRenderer::drawRgb565RleScaled(iconX, iconY, MenuAssets::FRAME_W, MenuAssets::FRAME_H,
                                               MenuAssets::BOX_ICON_RLE, offset, length, iconScale);
            leftW = ICON_SLOT_W;
        } else {
            // 未配置图标的子菜单继续使用统一尺寸色块。
            int drawBoxW = (int)(BOX_W * relScale);
            int drawBoxH = (int)(BOX_H * relScale);
            PixelRenderer::fillRect(boxX, y - drawBoxH / 2, drawBoxW, drawBoxH, boxColor);
            leftW = drawBoxW;
        }

        // 右侧说明文字，与色块垂直中心对齐；未选中时半透明（灰色）
        char label[24];
        const char* desc = itemLabel(i, label, sizeof(label));
        PixelRenderer::applyTextStyle(fs);
        int tw = canvas.textWidth(desc);
        int th = (int)(8 * fs);
        int descX = (boxX + leftW + (int)(10 * fs) + Hal::DISPLAY_W) / 2 - 10;
        int descY = y - th / 2;
        PixelRenderer::drawPixelText(descX - tw / 2, descY, desc, descColor, fs);
    }
}

void MenuScene::drawSimpleList() {
    float fs = PixelRenderer::getContentFontScale();
    int rowStep = (int)(14 * fs);
    int startY = 8;
    int sepGap = (int)(4 * fs);
    int count = itemCount();
    LGFX_Sprite& canvas = Hal::ins().canvas();

    for (int i = 0; i < count; i++) {
        int y = startY + i * rowStep;

        bool disabled = false;
        if (mode == Mode::TOY && i == TOY_BALL) {
            disabled = !GameEngine::ins().isToyStyleUnlocked(GameEngine::TOY_BALL) ||
                       (GameEngine::ins().getActiveToyBallDurability() == 0 &&
                        GameEngine::ins().getToyBallCount() == 0);
        }
        uint16_t color = disabled ? PixelRenderer::GRAY :
                         ((i == selected) ? PixelRenderer::YELLOW : PixelRenderer::WHITE);
        if (disabled && i == selected) color = PixelRenderer::rgb565(180, 180, 0);

        if (i == selected) {
            PixelRenderer::fillRect(4, y, 4, (int)(8 * fs),
                                    disabled ? PixelRenderer::GRAY : PixelRenderer::YELLOW);
        }

        char label[24];
        int textX = 14;
        if (mode == Mode::TOY) {
            textX = 22;
            if (i < TOY_ITEM_COUNT - 1 && i == GameEngine::ins().getToyStyle() && !disabled) {
                int cx = 14;
                int cy = y + (int)(4 * fs);
                canvas.fillTriangle(cx, cy - 3, cx + 3, cy, cx, cy + 3, PixelRenderer::CYAN);
                canvas.fillTriangle(cx, cy - 3, cx, cy + 3, cx - 3, cy, PixelRenderer::CYAN);
            }
        }
        PixelRenderer::drawPixelText(textX, y, itemLabel(i, label, sizeof(label)), color, fs);

        if (i < count - 1) {
            PixelRenderer::fillRect(4, y + rowStep - sepGap, Hal::DISPLAY_W - 8, 1, PixelRenderer::GRAY);
        }
    }
}

void MenuScene::drawFightList() {
    drawSimpleList();
}

void MenuScene::drawExploreList() {
    float fs = PixelRenderer::getContentFontScale();
    int rowStep = (int)(14 * fs);
    int startY = 10;
    int sepGap = (int)(4 * fs);
    bool exploreAvailable = GameEngine::ins().canExplore();
    bool night = GameEngine::ins().getTimeOfDay() == GameEngine::TIME_EVENING;
    LGFX_Sprite& canvas = Hal::ins().canvas();

    for (int i = 0; i < EXPLORE_ITEM_COUNT; i++) {
        int y = startY + i * rowStep;
        bool isBack = (i == LOCATION_BACK);
        bool isCup = (i == LOCATION_CUP);
        bool forbidden = !isBack && !isCup && night && (i == LOCATION_BACK_HILL);
        bool disabled = isCup ? !isCupAvailable() : ((!isBack && !exploreAvailable) || forbidden);
        uint16_t color = disabled ? PixelRenderer::GRAY : PixelRenderer::WHITE;
        if (i == selected) color = disabled ? PixelRenderer::rgb565(180, 180, 0) : PixelRenderer::YELLOW;

        if (i == selected) {
            PixelRenderer::fillRect(4, y, 4, (int)(8 * fs),
                                    disabled ? PixelRenderer::GRAY : PixelRenderer::YELLOW);
        }

        char label[24];
        const char* baseLabel = itemLabel(i, label, sizeof(label));
        PixelRenderer::drawPixelText(14, y, baseLabel, color, fs);

        if (!isBack) {
            PixelRenderer::fillRect(4, y + rowStep - sepGap, Hal::DISPLAY_W - 8, 1, PixelRenderer::GRAY);
        }
    }
}

void MenuScene::drawDebugList() {
    float fs = PixelRenderer::getContentFontScale();
    int rowStep = (int)(14 * fs);
    int startY = 10;
    int sepGap = (int)(4 * fs);

    for (int i = 0; i < DEBUG_ITEM_COUNT; i++) {
        int y = startY + i * rowStep;
        bool isBack = (i == DEBUG_BACK);
        uint16_t color = (i == selected) ? PixelRenderer::YELLOW : PixelRenderer::WHITE;

        if (i == selected) {
            PixelRenderer::fillRect(4, y, 4, (int)(8 * fs), PixelRenderer::YELLOW);
        }

        char label[24];
        const char* baseLabel = itemLabel(i, label, sizeof(label));
        PixelRenderer::drawPixelText(14, y, baseLabel, color, fs);

        if (!isBack) {
            PixelRenderer::fillRect(4, y + rowStep - sepGap, Hal::DISPLAY_W - 8, 1, PixelRenderer::GRAY);
        }
    }
}

void MenuScene::drawAttrEditDialog() {
    LGFX_Sprite& canvas = Hal::ins().canvas();
    float fs = PixelRenderer::getContentFontScale();

    int boxW = 154;
    int boxH = 70;
    int boxX = (Hal::DISPLAY_W - boxW) / 2;
    int boxY = (Hal::DISPLAY_H - boxH) / 2;
    PixelRenderer::fillRect(boxX, boxY, boxW, boxH, PixelRenderer::rgb565(12, 12, 18));
    canvas.drawRect(boxX, boxY, boxW, boxH, PixelRenderer::WHITE);

    char title[24];
    snprintf(title, sizeof(title), UiStrings::MENU_ATTR_EDIT_TITLE_FMT,
             attrName(attrEditIndex), attrEditValue,
             GameEngine::ins().getBug().debugGetAttrCap(attrEditIndex));
    PixelRenderer::applyTextStyle(fs);
    int titleW = canvas.textWidth(title);
    PixelRenderer::drawPixelText(boxX + (boxW - titleW) / 2, boxY + 10,
                                 title, PixelRenderer::WHITE, fs);

    static constexpr int BTN_COUNT = 3;
    const char* labels[BTN_COUNT] = {UiStrings::MINUS, UiStrings::PLUS, UiStrings::YES_LOWER};
    int btnW[BTN_COUNT] = {34, 34, 46};
    int gap = 8;
    int totalW = btnW[0] + btnW[1] + btnW[2] + gap * 2;
    int x = boxX + (boxW - totalW) / 2;
    int y = boxY + 42;
    int btnH = 18;
    for (int i = 0; i < BTN_COUNT; i++) {
        bool focused = (i == attrEditButton);
        uint16_t border = focused ? PixelRenderer::YELLOW : PixelRenderer::GRAY;
        uint16_t text = focused ? PixelRenderer::YELLOW : PixelRenderer::WHITE;
        PixelRenderer::fillRect(x, y, btnW[i], btnH, PixelRenderer::rgb565(24, 24, 30));
        canvas.drawRect(x, y, btnW[i], btnH, border);
        PixelRenderer::applyTextStyle(fs);
        int tw = canvas.textWidth(labels[i]);
        PixelRenderer::drawPixelText(x + (btnW[i] - tw) / 2,
                                     y + (btnH - (int)(8 * fs)) / 2,
                                     labels[i], text, fs);
        x += btnW[i] + gap;
    }
}

void MenuScene::drawGiftQuantityDialog() {
    LGFX_Sprite& canvas = Hal::ins().canvas();
    float fs = PixelRenderer::getContentFontScale();

    int boxW = 166;
    int boxH = 70;
    int boxX = (Hal::DISPLAY_W - boxW) / 2;
    int boxY = (Hal::DISPLAY_H - boxH) / 2;
    PixelRenderer::fillRect(boxX, boxY, boxW, boxH, PixelRenderer::rgb565(12, 12, 18));
    canvas.drawRect(boxX, boxY, boxW, boxH, PixelRenderer::WHITE);

    char title[32];
    snprintf(title, sizeof(title), UiStrings::GIFT_AMOUNT_TITLE_FMT,
             ItemCatalog::name(giftQuantityItemId), giftQuantityValue, giftQuantityMax);
    PixelRenderer::applyTextStyle(fs);
    int titleW = canvas.textWidth(title);
    PixelRenderer::drawPixelText(boxX + (boxW - titleW) / 2, boxY + 10,
                                 title, PixelRenderer::WHITE, fs);

    static constexpr int BTN_COUNT = 3;
    const char* labels[BTN_COUNT] = {UiStrings::MINUS, UiStrings::PLUS, UiStrings::YES_LOWER};
    int btnW[BTN_COUNT] = {34, 34, 46};
    int gap = 8;
    int totalW = btnW[0] + btnW[1] + btnW[2] + gap * 2;
    int x = boxX + (boxW - totalW) / 2;
    int y = boxY + 42;
    int btnH = 18;
    for (int i = 0; i < BTN_COUNT; i++) {
        bool focused = (i == giftQuantityButton);
        uint16_t border = focused ? PixelRenderer::YELLOW : PixelRenderer::GRAY;
        uint16_t text = focused ? PixelRenderer::YELLOW : PixelRenderer::WHITE;
        PixelRenderer::fillRect(x, y, btnW[i], btnH, PixelRenderer::rgb565(24, 24, 30));
        canvas.drawRect(x, y, btnW[i], btnH, border);
        PixelRenderer::applyTextStyle(fs);
        int tw = canvas.textWidth(labels[i]);
        PixelRenderer::drawPixelText(x + (btnW[i] - tw) / 2,
                                     y + (btnH - (int)(8 * fs)) / 2,
                                     labels[i], text, fs);
        x += btnW[i] + gap;
    }
}
