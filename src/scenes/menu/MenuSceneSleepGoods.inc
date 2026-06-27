void MenuScene::drawSleepConfirm() {
    float fs = PixelRenderer::getContentFontScale();
    LGFX_Sprite& canvas = Hal::ins().canvas();

    // 背景遮罩
    PixelRenderer::fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H,
                            PixelRenderer::rgb565(0, 0, 0));

    // 提示文字
    const char* msg = UiStrings::SLEEP_CONFIRM;
    int lineCount = 1;
    int maxLineW = 0;
    const char* p = msg;
    const char* lineStart = msg;
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
    int boxH = lineCount * lineH + padY * 2 + (int)(14 * fs);  // 额外空间给导航提示
    int boxX = (Hal::DISPLAY_W - boxW) / 2;
    int boxY = (Hal::DISPLAY_H - boxH) / 2;

    PixelRenderer::fillRect(boxX, boxY, boxW, boxH, PixelRenderer::rgb565(20, 20, 30));
    canvas.drawRect(boxX, boxY, boxW, boxH, PixelRenderer::WHITE);

    int y = boxY + padY;
    p = msg;
    lineStart = msg;
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

    // 导航提示
    PixelRenderer::applyTextStyle(fs);
    int navW = canvas.textWidth(UiStrings::SLEEP_NAV);
    PixelRenderer::drawPixelText(boxX + (boxW - navW) / 2,
                                 y + (int)(2 * fs),
                                 UiStrings::SLEEP_NAV,
                                 PixelRenderer::GRAY, fs);
}

void MenuScene::startSleepTransition() {
    toastMsg = nullptr;
    sleepTransitionBaseBrightness = Hal::ins().getBrightness();
    sleepTransitionStartMs = Hal::ins().millis();
    sleepTransitionActive = true;
    Hal::ins().setBrightness(sleepTransitionBaseBrightness);
}

uint8_t MenuScene::sleepTransitionBrightness(uint32_t elapsedMs) const {
    if (elapsedMs < SLEEP_FADE_MS) {
        uint32_t remaining = SLEEP_FADE_MS - elapsedMs;
        return (uint8_t)((uint32_t)sleepTransitionBaseBrightness * remaining / SLEEP_FADE_MS);
    }
    if (elapsedMs < SLEEP_FADE_MS + SLEEP_HOLD_MS) {
        return 0;
    }
    uint32_t fadeInElapsed = elapsedMs - SLEEP_FADE_MS - SLEEP_HOLD_MS;
    if (fadeInElapsed > SLEEP_FADE_MS) fadeInElapsed = SLEEP_FADE_MS;
    return (uint8_t)((uint32_t)sleepTransitionBaseBrightness * fadeInElapsed / SLEEP_FADE_MS);
}

void MenuScene::drawSleepTransition() {
    PixelRenderer::fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::BLACK);

    float fs = PixelRenderer::getContentFontScale();
    if (fs < 1.6f) fs = 1.6f;
    const char* text = UiStrings::SLEEP_ZZZ;
    LGFX_Sprite& canvas = Hal::ins().canvas();
    PixelRenderer::applyTextStyle(fs);
    int w = canvas.textWidth(text);
    int x = (Hal::DISPLAY_W - w) / 2;
    int y = (Hal::DISPLAY_H - (int)(12 * fs)) / 2;
    PixelRenderer::drawPixelText(x, y, text, PixelRenderer::WHITE, fs);
}

int MenuScene::descriptionLineStep(float fs) const {
    return (fs < 1.65f) ? 16 : 22;
}

void MenuScene::updateDescriptionScroll(int scrollKey, int maxScroll) {
    if (scrollKey != descScrollKey) {
        descScrollKey = scrollKey;
        descScroll = 0.0f;
        descScrollLastMs = Hal::ins().millis();
    }

    if (maxScroll <= 0) {
        descScroll = 0.0f;
        descScrollLastMs = Hal::ins().millis();
        return;
    }

    uint32_t now = Hal::ins().millis();
    uint32_t dt = now - descScrollLastMs;
    if (dt > 120) dt = 120;
    descScrollLastMs = now;

    float ax, ay, az;
    Hal::ins().getAccel(ax, ay, az);
    float mag = Hal::ins().getAccelMagnitude();
    if (fabsf(ay) > 1.5f || mag > 2.0f) return;

    static constexpr float TILT_DEADZONE_G = 0.22f;
    if (fabsf(ax) <= TILT_DEADZONE_G) return;

    float strength = (fabsf(ax) - TILT_DEADZONE_G) / 0.55f;
    if (strength > 1.0f) strength = 1.0f;

    float dir = (ax < 0.0f) ? 1.0f : -1.0f;
    descScroll += dir * strength * 58.0f * ((float)dt / 1000.0f);
    if (descScroll < 0.0f) descScroll = 0.0f;
    if (descScroll > (float)maxScroll) descScroll = (float)maxScroll;
}

void MenuScene::drawScrollableDescription(const char* const* lines, int lineCount,
                                          int x, int y, int w, uint16_t color,
                                          float fs, int scrollKey) {
    if (lineCount <= 0 || w <= 0) return;

    LGFX_Sprite& canvas = Hal::ins().canvas();
    PixelRenderer::applyTextStyle(fs);

    int maxLineW = 0;
    for (int i = 0; i < lineCount; ++i) {
        int lineW = canvas.textWidth(lines[i]);
        if (lineW > maxLineW) maxLineW = lineW;
    }

    int maxScroll = maxLineW > w ? maxLineW - w + 4 : 0;
    updateDescriptionScroll(scrollKey, maxScroll);

    int lineStep = descriptionLineStep(fs);
    int h = lineCount * lineStep;
    canvas.setClipRect(x, y - 1, w, h + 2);
    for (int i = 0; i < lineCount; ++i) {
        PixelRenderer::drawPixelText(x - (int)descScroll, y + i * lineStep,
                                     lines[i], color, fs);
    }
    canvas.clearClipRect();
}

void MenuScene::drawFoodLayout() {
    Bug& bug = GameEngine::ins().getBug();
    LGFX_Sprite& canvas = Hal::ins().canvas();
    float fs = PixelRenderer::getContentFontScale();
    bool giftMode = (mode == Mode::GIFT_FOOD);
    int listCount = itemCount();

    static constexpr int LEFT_W = 78;
    static constexpr int RIGHT_X = LEFT_W + 8;
    static constexpr int LIST_Y_START = 22;
    static constexpr int LIST_BOTTOM = Hal::DISPLAY_H - 8;
    static constexpr int VISIBLE_H = LIST_BOTTOM - LIST_Y_START;
    static constexpr int ROW_H = 22;          // 行高（含空隙），方便后续扩展更多食物
    static constexpr int TEXT_X = 12;
    static constexpr int MARK_X_OFS = 6;      // 文本与选择标记之间的距离

    // ---- 左侧可滚动列表 ----
    int totalH = listCount * ROW_H;
    int maxScroll = (totalH > VISIBLE_H) ? (totalH - VISIBLE_H) : 0;
    int targetScroll = selected * ROW_H - VISIBLE_H / 2 + ROW_H / 2;
    if (targetScroll < 0) targetScroll = 0;
    if (targetScroll > maxScroll) targetScroll = maxScroll;

    // 平滑滚动
    float diff = (float)targetScroll - foodScroll;
    if (fabsf(diff) < 0.5f) {
        foodScroll = (float)targetScroll;
    } else {
        foodScroll += diff * 0.25f;
    }

    // 左右分隔线
    canvas.drawFastVLine(LEFT_W, 20, Hal::DISPLAY_H - 24, PixelRenderer::GRAY);

    // 绘制每个 item，仅绘制可见区域内的行
    for (int i = 0; i < listCount; i++) {
        int y = LIST_Y_START + i * ROW_H - (int)foodScroll;
        if (y + ROW_H < LIST_Y_START || y > LIST_BOTTOM) continue;

        bool isSelected = (i == selected);
        bool isFood = !isVisibleFoodBackIndex(i);
        FoodType ft = isFood ? visibleFoodAt(i) : FoodType::DROP;
        uint8_t count = isFood ? bug.getFoodCount(ft) : 1;
        bool unlocked = !isFood || giftMode || GameEngine::ins().isFoodUnlocked(ft);
        bool hasStock = (count > 0);

        // 未解锁或无库存的食物显示为灰色；选中时统一高亮为黄色，但不可用项的黄色更灰一些
        uint16_t color;
        if (isSelected) {
            color = (unlocked && hasStock) ? PixelRenderer::YELLOW : PixelRenderer::rgb565(180, 180, 0);
        } else {
            color = (unlocked && hasStock) ? PixelRenderer::WHITE : PixelRenderer::GRAY;
        }

        const char* label;
        if (!isFood) {
            label = UiStrings::BACK;
        } else {
            label = FoodTypeInfo::name(ft);
        }

        int textY = y + (ROW_H - (int)(8 * fs)) / 2;

        // 全局食物标记：在当前设置的食物名字前面绘制一个青色小菱形
        // 无库存的食物不能挂上菱形标记，也不能被选作全局食物
        int globalFood = GameEngine::ins().getFoodStyle();
        if (!giftMode && isFood && (uint8_t)ft == globalFood && unlocked && hasStock) {
            int cx = TEXT_X - 8;
            int cy = textY + (int)(4 * fs);
            uint16_t diamondColor = PixelRenderer::CYAN;
            // 确认反馈期间菱形也短暂变绿
            if (foodConfirmTime > 0 && GameEngine::ins().getGameNow() < foodConfirmTime && isSelected) {
                diamondColor = PixelRenderer::GREEN;
            }
            canvas.fillTriangle(cx, cy - 3, cx + 3, cy, cx, cy + 3, diamondColor);
            canvas.fillTriangle(cx, cy - 3, cx, cy + 3, cx - 3, cy, diamondColor);
        }

        PixelRenderer::drawPixelText(TEXT_X, textY, label, color, fs);
    }

    // ---- 右侧详情面板 ----
    if (selected >= 0 && selected < listCount && !isVisibleFoodBackIndex(selected)) {
        FoodType ft = visibleFoodAt(selected);
        int idx = (int)ft;
        bool unlocked = giftMode || GameEngine::ins().isFoodUnlocked(ft);

        // 菜单预览保持独立视觉大小，不跟随主界面食物资源放大。
        uint16_t foodOffset = pgm_read_word(&FoodAssets::SPRITE_FRAMES[idx].offset);
        uint16_t foodLength = pgm_read_word(&FoodAssets::SPRITE_FRAMES[idx].length);
        static constexpr float FOOD_MENU_ICON_SCALE = 2.0f / 1.2f;
        float iconScale = FOOD_MENU_ICON_SCALE;
        int iconW = (int)(FoodAssets::FRAME_W * iconScale);
        int iconH = (int)(FoodAssets::FRAME_H * iconScale);

        const char* name = FoodTypeInfo::name(ft);
        uint8_t count = bug.getFoodCount(ft);
        char storageBuf[16];
        if (!unlocked) {
            snprintf(storageBuf, sizeof(storageBuf), "%s", UiStrings::LOCKED);
        } else {
            snprintf(storageBuf, sizeof(storageBuf), UiStrings::COUNT_X_FMT, count);
        }

        PixelRenderer::applyTextStyle(fs);
        int nameW = canvas.textWidth(name);
        int storageW = canvas.textWidth(storageBuf);
        int textW = (nameW > storageW) ? nameW : storageW;

        // icon 距离左侧分割线 5px，文字紧跟 icon 右侧
        int iconX = RIGHT_X + 5;
        int iconY = 18;
        int textX = iconX + iconW + 5;
        int nameY = iconY + 2;
        int storageY = nameY + (int)(12 * fs) - 2;

        PixelRenderer::drawRgb565RleScaled(iconX, iconY,
                                           FoodAssets::FRAME_W,
                                           FoodAssets::FRAME_H,
                                           FoodAssets::SPRITE_RLE,
                                           foodOffset, foodLength, iconScale, false);

        // 名字在右上，库存在右下
        PixelRenderer::drawPixelText(textX, nameY, name, PixelRenderer::WHITE, fs);
        PixelRenderer::drawPixelText(textX, storageY, storageBuf,
                                     (unlocked && count > 0) ? PixelRenderer::CREAM : PixelRenderer::GRAY, fs);

        // 描述：放在容器下方，严格限制在右侧面板内
        int descX = RIGHT_X + 2;
        int descY = iconY + iconH + 12;
        int descW = Hal::DISPLAY_W - descX - 2;
        const char* descLines[3] = {
            FoodTypeInfo::descLine1(ft),
            FoodTypeInfo::descLine2(ft),
            FoodTypeInfo::descLine3(ft),
        };
        drawScrollableDescription(descLines, 3, descX, descY, descW,
                                  unlocked ? PixelRenderer::CREAM : PixelRenderer::GRAY, fs, 0x100 + idx);
    } else if (selected == listCount - 1) {
        // Back 选中时的右侧提示
        PixelRenderer::drawPixelText(RIGHT_X + 20, 80,
                                     giftMode ? UiStrings::BACK_TO_GIFT : UiStrings::BACK_TO_BOX,
                                     PixelRenderer::GRAY, fs);
    }
}


void MenuScene::drawBowlLayout() {
    Bug& bug = GameEngine::ins().getBug();
    LGFX_Sprite& canvas = Hal::ins().canvas();
    float fs = PixelRenderer::getContentFontScale();

    static constexpr int LEFT_W = 78;
    static constexpr int RIGHT_X = LEFT_W + 8;
    static constexpr int LIST_Y_START = 10;
    static constexpr int LIST_BOTTOM = Hal::DISPLAY_H - 8;
    static constexpr int VISIBLE_H = LIST_BOTTOM - LIST_Y_START;
    static constexpr int ROW_H = 22;
    static constexpr int TEXT_X = 12;

    // ---- 左侧可滚动列表 ----
    int totalH = BOWL_ITEM_COUNT * ROW_H;
    int maxScroll = (totalH > VISIBLE_H) ? (totalH - VISIBLE_H) : 0;
    int targetScroll = selected * ROW_H - VISIBLE_H / 2 + ROW_H / 2;
    if (targetScroll < 0) targetScroll = 0;
    if (targetScroll > maxScroll) targetScroll = maxScroll;

    float diff = (float)targetScroll - bowlScroll;
    if (fabsf(diff) < 0.5f) {
        bowlScroll = (float)targetScroll;
    } else {
        bowlScroll += diff * 0.25f;
    }

    canvas.drawFastVLine(LEFT_W, 8, Hal::DISPLAY_H - 16, PixelRenderer::GRAY);

    for (int i = 0; i < BOWL_ITEM_COUNT; i++) {
        int y = LIST_Y_START + i * ROW_H - (int)bowlScroll;
        if (y + ROW_H < LIST_Y_START || y > LIST_BOTTOM) continue;

        bool isSelected = (i == selected);
        bool isStyle = (i < BOWL_ITEM_COUNT - 1);
        bool unlocked = isStyle && GameEngine::ins().isBowlStyleUnlocked((uint8_t)i);

        // 未解锁的风格显示为灰色；选中时统一高亮为黄色，未解锁的黄色更灰
        uint16_t color;
        if (isSelected) {
            color = unlocked ? PixelRenderer::YELLOW : PixelRenderer::rgb565(180, 180, 0);
        } else {
            color = unlocked ? PixelRenderer::WHITE : PixelRenderer::GRAY;
        }

        const char* label;
        if (isStyle) {
            label = BowlAssets::NAME[i];
        } else {
            label = UiStrings::BACK;
        }

        int textY = y + (ROW_H - (int)(8 * fs)) / 2;

        // 当前食物盘风格菱形标记（只显示在已解锁的当前风格上）
        int currentStyle = GameEngine::ins().getBowlStyle();
        if (isStyle && i == currentStyle && unlocked) {
            int cx = TEXT_X - 8;
            int cy = textY + (int)(4 * fs);
            canvas.fillTriangle(cx, cy - 3, cx + 3, cy, cx, cy + 3, PixelRenderer::CYAN);
            canvas.fillTriangle(cx, cy - 3, cx, cy + 3, cx - 3, cy, PixelRenderer::CYAN);
        }

        PixelRenderer::drawPixelText(TEXT_X, textY, label, color, fs);
    }

    // ---- 右侧详情面板 ----
    if (selected >= 0 && selected < BOWL_ITEM_COUNT - 1) {
        int idx = selected;
        bool unlocked = GameEngine::ins().isBowlStyleUnlocked((uint8_t)idx);

        uint16_t bowlOffset = pgm_read_word(&BowlAssets::SPRITE_FRAMES[idx].offset);
        uint16_t bowlLength = pgm_read_word(&BowlAssets::SPRITE_FRAMES[idx].length);
        static constexpr float BOWL_MENU_ICON_SCALE = 2.0f / 1.5f;
        float iconScale = BOWL_MENU_ICON_SCALE;
        int iconW = (int)(BowlAssets::FRAME_W * iconScale);
        int iconH = (int)(BowlAssets::FRAME_H * iconScale);

        const char* name = BowlAssets::NAME[idx];
        char storageBuf[16];
        snprintf(storageBuf, sizeof(storageBuf), "%s",
                 unlocked ? UiStrings::READY : UiStrings::LOCKED);

        PixelRenderer::applyTextStyle(fs);
        int nameW = canvas.textWidth(name);
        int storageW = canvas.textWidth(storageBuf);
        int textW = (nameW > storageW) ? nameW : storageW;

        // 容器整体靠左对齐：icon 贴右侧面板左边缘，名字/状态紧贴 icon
        int iconX = RIGHT_X;
        int iconY = 7;
        int textX = iconX + iconW;
        int nameY = iconY + 2;
        int storageY = nameY + (int)(12 * fs);

        PixelRenderer::drawRgb565RleScaled(iconX, iconY,
                                           BowlAssets::FRAME_W,
                                           BowlAssets::FRAME_H,
                                           BowlAssets::SPRITE_RLE,
                                           bowlOffset, bowlLength, iconScale, false);

        // 名字在右上，状态在下
        PixelRenderer::drawPixelText(textX, nameY, name, PixelRenderer::WHITE, fs);
        PixelRenderer::drawPixelText(textX, storageY, storageBuf,
                                     unlocked ? PixelRenderer::CREAM : PixelRenderer::GRAY, fs);

        // 描述：三行，底部留出第三行空间
        int descX = RIGHT_X + 2;
        int descY = iconY + iconH + 12;
        int descW = Hal::DISPLAY_W - descX - 2;
        const char* descLines[3] = {
            BowlAssets::DESC_LINE1[idx],
            BowlAssets::DESC_LINE2[idx],
            BowlAssets::DESC_LINE3[idx],
        };
        drawScrollableDescription(descLines, 3, descX, descY, descW,
                                  PixelRenderer::CREAM, fs, 0x200 + idx);
    } else if (selected == BOWL_ITEM_COUNT - 1) {
        PixelRenderer::drawPixelText(RIGHT_X + 20, 62, UiStrings::BACK_TO_BOX,
                                     PixelRenderer::GRAY, fs);
    }
}

void MenuScene::drawWoodLayout() {
    Bug& bug = GameEngine::ins().getBug();
    LGFX_Sprite& canvas = Hal::ins().canvas();
    float fs = PixelRenderer::getContentFontScale();

    static constexpr int LEFT_W = 78;
    static constexpr int RIGHT_X = LEFT_W + 8;
    static constexpr int LIST_Y_START = 10;
    static constexpr int LIST_BOTTOM = Hal::DISPLAY_H - 8;
    static constexpr int VISIBLE_H = LIST_BOTTOM - LIST_Y_START;
    static constexpr int ROW_H = 22;
    static constexpr int TEXT_X = 12;

    // ---- 左侧可滚动列表 ----
    int totalH = WOOD_ITEM_COUNT * ROW_H;
    int maxScroll = (totalH > VISIBLE_H) ? (totalH - VISIBLE_H) : 0;
    int targetScroll = selected * ROW_H - VISIBLE_H / 2 + ROW_H / 2;
    if (targetScroll < 0) targetScroll = 0;
    if (targetScroll > maxScroll) targetScroll = maxScroll;

    float diff = (float)targetScroll - woodScroll;
    if (fabsf(diff) < 0.5f) {
        woodScroll = (float)targetScroll;
    } else {
        woodScroll += diff * 0.25f;
    }

    canvas.drawFastVLine(LEFT_W, 8, Hal::DISPLAY_H - 16, PixelRenderer::GRAY);

    for (int i = 0; i < WOOD_ITEM_COUNT; i++) {
        int y = LIST_Y_START + i * ROW_H - (int)woodScroll;
        if (y + ROW_H < LIST_Y_START || y > LIST_BOTTOM) continue;

        bool isSelected = (i == selected);
        bool isStyle = (i < WOOD_ITEM_COUNT - 1);
        uint8_t styleIdx = (i == WOOD_NONE) ? 0xFF : (uint8_t)(i - 1);
        bool unlocked = (i == WOOD_NONE) || (isStyle && bug.isWoodUnlocked(styleIdx));
        uint16_t color;
        if (isSelected) {
            color = (!isStyle || unlocked) ? PixelRenderer::YELLOW : PixelRenderer::rgb565(180, 180, 0);
        } else {
            color = (!isStyle || unlocked) ? PixelRenderer::WHITE : PixelRenderer::GRAY;
        }

        const char* label;
        if (i == WOOD_NONE) {
            label = UiStrings::WOOD_NONE;
        } else if (isStyle) {
            label = WoodAssets::NAME[styleIdx];
        } else {
            label = UiStrings::BACK;
        }

        int textY = y + (ROW_H - (int)(8 * fs)) / 2;

        // 当前腐木风格菱形标记（None 不显示）
        int currentStyle = (int)GameEngine::ins().getWoodStyle();
        if (i != WOOD_NONE && isStyle && styleIdx == currentStyle && unlocked) {
            int cx = TEXT_X - 8;
            int cy = textY + (int)(4 * fs);
            canvas.fillTriangle(cx, cy - 3, cx + 3, cy, cx, cy + 3, PixelRenderer::CYAN);
            canvas.fillTriangle(cx, cy - 3, cx, cy + 3, cx - 3, cy, PixelRenderer::CYAN);
        }

        PixelRenderer::drawPixelText(TEXT_X, textY, label, color, fs);
    }

    // ---- 右侧详情面板 ----
    if (selected >= 0 && selected < WOOD_ITEM_COUNT - 1) {
        if (selected == WOOD_NONE) {
            // 空选项：右侧显示简短说明
            PixelRenderer::drawPixelText(RIGHT_X + 10, 40,
                                         UiStrings::WOOD_NONE_PLACED,
                                         PixelRenderer::GRAY, fs);
            return;
        }

        int idx = selected - 1;
        bool unlocked = bug.isWoodUnlocked((uint8_t)idx);

        uint16_t woodOffset = pgm_read_word(&WoodAssets::SPRITE_FRAMES[idx].offset);
        uint16_t woodLength = pgm_read_word(&WoodAssets::SPRITE_FRAMES[idx].length);
        static constexpr float WOOD_ICON_SCALE = 1.0f / 1.5f;
        float iconScale = WOOD_ICON_SCALE;
        int iconW = (int)(WoodAssets::FRAME_W * iconScale);
        int iconH = (int)(WoodAssets::FRAME_H * iconScale);

        const char* name = WoodAssets::NAME[idx];
        char storageBuf[16];
        snprintf(storageBuf, sizeof(storageBuf), "%s",
                 bug.isWoodUnlocked((uint8_t)idx) ? UiStrings::UNLOCKED : UiStrings::LOCKED);

        PixelRenderer::applyTextStyle(fs);
        int nameW = canvas.textWidth(name);
        int storageW = canvas.textWidth(storageBuf);
        int textW = (nameW > storageW) ? nameW : storageW;

        // 容器整体居中但保持右侧边距：icon + 间距 + 右侧文字（名字在上，库存在下）
        int rightW = Hal::DISPLAY_W - RIGHT_X;
        int contentW = iconW + 5 + textW;
        int contentX = RIGHT_X + (rightW - contentW) / 2;
        if (contentX < RIGHT_X + 2) contentX = RIGHT_X + 2;
        int iconX = contentX;
        int iconY = 3;
        int textX = iconX + iconW + 5;
        // 文字块在 icon 右侧垂直居中
        int textBlockH = (int)(8 * fs) + (int)(12 * fs);
        int nameY = iconY + (iconH - textBlockH) / 2;
        int storageY = nameY + (int)(12 * fs);

        PixelRenderer::drawRgb565RleScaled(iconX, iconY,
                                           WoodAssets::FRAME_W,
                                           WoodAssets::FRAME_H,
                                           WoodAssets::SPRITE_RLE,
                                           woodOffset, woodLength, iconScale, false);

        // 名字在右上，库存在右下
        PixelRenderer::drawPixelText(textX, nameY, name, unlocked ? PixelRenderer::WHITE : PixelRenderer::GRAY, fs);
        PixelRenderer::drawPixelText(textX, storageY, storageBuf,
                                     unlocked ? PixelRenderer::CREAM : PixelRenderer::GRAY, fs);

        // 描述：三行，底部留出第三行空间
        int descX = RIGHT_X + 2;
        int descY = iconY + iconH + 12;
        uint16_t descColor = unlocked ? PixelRenderer::CREAM : PixelRenderer::GRAY;
        int descW = Hal::DISPLAY_W - descX - 2;
        const char* descLines[3] = {
            WoodAssets::DESC_LINE1[idx],
            WoodAssets::DESC_LINE2[idx],
            WoodAssets::DESC_LINE3[idx],
        };
        drawScrollableDescription(descLines, 3, descX, descY, descW,
                                  descColor, fs, 0x300 + idx);
    } else {
        PixelRenderer::drawPixelText(RIGHT_X + 20, 62, UiStrings::BACK_TO_BOX,
                                     PixelRenderer::GRAY, fs);
    }
}
