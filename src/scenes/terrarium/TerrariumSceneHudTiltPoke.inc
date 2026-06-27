void TerrariumScene::drawStatusBar() {
    Bug& bug = GameEngine::ins().getBug();
    if (GameEngine::ins().isVisitGuest()) return;

    LGFX_Sprite& canvas = Hal::ins().canvas();
    float fs = PixelRenderer::getContentFontScale();

    static constexpr int HUD_Y = 2;
    int textH = (int)(8 * fs + 0.5f);

    auto drawIcon = [&](int x, int y, const uint16_t* rows, int w, int h, uint16_t color) {
        for (int row = 0; row < h; row++) {
            uint16_t mask = rows[row];
            for (int col = 0; col < w; col++) {
                if (mask & (1 << (w - 1 - col))) {
                    PixelRenderer::fillRect(x + col, y + row, 1, 1, color);
                }
            }
        }
    };

    // ---- 极简 HUD：时间浮在右上角，状态压缩在下方 ----
    char clockBuf[8];
    GameEngine::ins().getExploreClockText(clockBuf, sizeof(clockBuf));
    PixelRenderer::applyTextStyle(fs);
    int timeW = canvas.textWidth(clockBuf);
    int timeX = Hal::DISPLAY_W - timeW - 4;
    int timeY = HUD_Y;

    uint8_t mot = bug.getMot();
    static const uint16_t HEART_MASK[9] = {
        0b000000000,
        0b011000110,
        0b111101111,
        0b111111111,
        0b111111111,
        0b011111110,
        0b001111100,
        0b000111000,
        0b000010000,
    };
    static constexpr int ICON_SIZE = 9;
    static constexpr int ICON_GAP = 4;
    int iconY = HUD_Y + textH + 4;
    int weatherX = Hal::DISPLAY_W - ICON_SIZE - 5;
    int heartX = weatherX - ICON_GAP - ICON_SIZE;
    int totalRows = ICON_SIZE;

    auto drawHeartMeter = [&](int x, int y, uint8_t value) {
        int rows = (value * totalRows + 50) / 100;
        if (rows < 1 && value > 0) rows = 1;
        if (rows > totalRows) rows = totalRows;
        for (int row = 0; row < totalRows; row++) {
            uint16_t mask = HEART_MASK[row];
            bool fill = (row >= totalRows - rows);
            uint16_t c = fill ? PixelRenderer::RED : PixelRenderer::GRAY;
            for (int col = 0; col < ICON_SIZE; col++) {
                if (mask & (1 << (ICON_SIZE - 1 - col))) {
                    PixelRenderer::fillRect(x + col, y + row, 1, 1, c);
                }
            }
        }
    };
    auto hungerColor = [](uint8_t hunger) {
        return hunger > 50 ? PixelRenderer::GREEN :
               (hunger > 20 ? PixelRenderer::YELLOW : PixelRenderer::RED);
    };
    auto drawHourglassMeter = [&](int x, int y, uint32_t remaining, uint32_t duration) {
        static const uint16_t HOURGLASS_MASK[9] = {
            0b111111111,
            0b011111110,
            0b001111100,
            0b000111000,
            0b000010000,
            0b000111000,
            0b001111100,
            0b011111110,
            0b111111111,
        };
        int rows = duration == 0 ? 0 : (int)(((uint64_t)remaining * totalRows + duration / 2) / duration);
        if (rows < 1 && remaining > 0) rows = 1;
        if (rows > totalRows) rows = totalRows;
        for (int row = 0; row < totalRows; row++) {
            uint16_t mask = HOURGLASS_MASK[row];
            bool fill = (row >= totalRows - rows);
            uint16_t c = fill ? PixelRenderer::YELLOW : PixelRenderer::GRAY;
            for (int col = 0; col < ICON_SIZE; col++) {
                if (mask & (1 << (ICON_SIZE - 1 - col))) {
                    PixelRenderer::fillRect(x + col, y + row, 1, 1, c);
                }
            }
        }
    };

    if (GameEngine::ins().isVisitHost()) {
        const VisitBugSnapshot& guest = GameEngine::ins().getVisitRemoteBug();
        static const uint16_t WEATHER_SUN_VISIT[9] = {
            0b100010001,
            0b010010010,
            0b001111100,
            0b011111110,
            0b111111111,
            0b011111110,
            0b001111100,
            0b010010010,
            0b100010001,
        };
        static const uint16_t WEATHER_CLOUD_VISIT[9] = {
            0b000000000,
            0b000111000,
            0b001111100,
            0b011111110,
            0b111111111,
            0b111111111,
            0b011111110,
            0b000000000,
            0b000000000,
        };
        int statusW = (weatherX + ICON_SIZE) - heartX;
        int hostHeartX = heartX;
        int guestHeartX = hostHeartX - ICON_GAP - statusW;
        int guestBarX = guestHeartX;
        int guestHourglassX = guestBarX + statusW - ICON_SIZE;
        if (guestHeartX < 0) {
            guestHeartX = 0;
            guestBarX = 0;
            guestHourglassX = statusW - ICON_SIZE;
        }
        int barY = iconY + ICON_SIZE + 3;
        int barH = 4;
        int panelX = (timeX < guestBarX ? timeX : guestBarX) - 4;
        if (panelX < 0) panelX = 0;
        int panelW = Hal::DISPLAY_W - panelX;
        int panelH = barY + barH + 3;
        for (int y = 0; y < panelH; y++) {
            for (int x = panelX; x < panelX + panelW; x++) {
                if (((x + y) & 1) == 0) {
                    PixelRenderer::fillRect(x, y, 1, 1, PixelRenderer::BLACK);
                }
            }
        }

        PixelRenderer::drawPixelText(timeX + 1, timeY + 1, clockBuf, PixelRenderer::BLACK, fs);
        PixelRenderer::drawPixelText(timeX, timeY, clockBuf, PixelRenderer::WHITE, fs);
        drawHeartMeter(guestHeartX, iconY, guest.motivation);
        drawHourglassMeter(guestHourglassX, iconY,
                           GameEngine::ins().getVisitRemainingMs(),
                           GameEngine::ins().getVisitDurationMs());
        drawHeartMeter(hostHeartX, iconY, bug.getMot());
        uint32_t gameDay = GameEngine::ins().getCurrentGameDay();
        bool cloudy = (gameDay % 3 == 0);
        drawIcon(weatherX, iconY,
                 cloudy ? WEATHER_CLOUD_VISIT : WEATHER_SUN_VISIT,
                 ICON_SIZE, ICON_SIZE,
                 cloudy ? PixelRenderer::WHITE : PixelRenderer::YELLOW);
        PixelRenderer::drawProgressBar(guestBarX, barY, statusW, barH,
                                       guest.hunger / 100.0f,
                                       hungerColor(guest.hunger),
                                       PixelRenderer::GRAY);
        PixelRenderer::drawProgressBar(heartX, barY, statusW, barH,
                                       bug.getHunger() / 100.0f,
                                       hungerColor(bug.getHunger()),
                                       PixelRenderer::GRAY);
        return;
    }

    int fillRows = (mot * totalRows + 50) / 100;
    if (fillRows < 1 && mot > 0) fillRows = 1;
    if (fillRows > totalRows) fillRows = totalRows;

    int barY = iconY + ICON_SIZE + 3;
    int barH = 4;
    int panelX = (timeX < heartX ? timeX : heartX) - 4;
    if (panelX < 0) panelX = 0;
    int panelY = 0;
    int panelW = Hal::DISPLAY_W - panelX;
    int panelH = barY + barH + 3;
    for (int y = panelY; y < panelY + panelH; y++) {
        for (int x = panelX; x < panelX + panelW; x++) {
            if (((x + y) & 1) == 0) {
                PixelRenderer::fillRect(x, y, 1, 1, PixelRenderer::BLACK);
            }
        }
    }

    PixelRenderer::drawPixelText(timeX + 1, timeY + 1, clockBuf, PixelRenderer::BLACK, fs);
    PixelRenderer::drawPixelText(timeX, timeY, clockBuf, PixelRenderer::WHITE, fs);

    for (int row = 0; row < totalRows; row++) {
        uint16_t mask = HEART_MASK[row];
        bool fill = (row >= totalRows - fillRows);
        uint16_t c = fill ? PixelRenderer::RED : PixelRenderer::GRAY;
        for (int col = 0; col < ICON_SIZE; col++) {
            if (mask & (1 << (ICON_SIZE - 1 - col))) {
                PixelRenderer::fillRect(heartX + col, iconY + row, 1, 1, c);
            }
        }
    }

    static const uint16_t WEATHER_SUN[9] = {
        0b100010001,
        0b010010010,
        0b001111100,
        0b011111110,
        0b111111111,
        0b011111110,
        0b001111100,
        0b010010010,
        0b100010001,
    };
    static const uint16_t WEATHER_CLOUD[9] = {
        0b000000000,
        0b000111000,
        0b001111100,
        0b011111110,
        0b111111111,
        0b111111111,
        0b011111110,
        0b000000000,
        0b000000000,
    };
    uint32_t gameDay = GameEngine::ins().getCurrentGameDay();
    bool cloudy = (gameDay % 3 == 0);
    const uint16_t* icon = cloudy ? WEATHER_CLOUD : WEATHER_SUN;
    uint16_t iconColor = cloudy ? PixelRenderer::WHITE : PixelRenderer::YELLOW;
    drawIcon(weatherX, iconY, icon, ICON_SIZE, ICON_SIZE, iconColor);

    uint16_t hColor = hungerColor(bug.getHunger());
    int barX = heartX;
    int barW = (weatherX + ICON_SIZE) - heartX;
    PixelRenderer::drawProgressBar(barX, barY, barW, barH, bug.getHunger() / 100.0f, hColor, PixelRenderer::GRAY);
}

void TerrariumScene::drawVisitAwayOverlay() {
    LGFX_Sprite& canvas = Hal::ins().canvas();
    uint32_t remainingMs = GameEngine::ins().getVisitRemainingMs();
    uint32_t minutes = (remainingMs + 59999UL) / 60000UL;
    if (minutes < 1 && remainingMs > 0) minutes = 1;

    char line2[24];
    snprintf(line2, sizeof(line2), UiStrings::VISIT_LEFT_FMT, (unsigned)minutes);
    const char* line3 = UiStrings::VISIT_RECALL_HINT;

    float titleFs = 2.0f;
    float bodyFs = PixelRenderer::getContentFontScale();
    int titleY = 56;
    int bodyY = 78;
    int hintY = 98;

    PixelRenderer::applyTextStyle(titleFs);
    int titleW = canvas.textWidth(UiStrings::VISIT_AWAY_TITLE);
    PixelRenderer::applyTextStyle(bodyFs);
    int bodyW = canvas.textWidth(line2);
    PixelRenderer::applyTextStyle(1.0f);
    int hintW = canvas.textWidth(line3);

    int titleX = (Hal::DISPLAY_W - titleW) / 2;
    int bodyX = (Hal::DISPLAY_W - bodyW) / 2;
    int hintX = (Hal::DISPLAY_W - hintW) / 2;

    PixelRenderer::fillRect(42, 45, 156, 72, PixelRenderer::BLACK);
    PixelRenderer::drawPixelText(titleX + 1, titleY + 1, UiStrings::VISIT_AWAY_TITLE, PixelRenderer::BLACK, titleFs);
    PixelRenderer::drawPixelText(titleX, titleY, UiStrings::VISIT_AWAY_TITLE, PixelRenderer::WHITE, titleFs);
    PixelRenderer::drawPixelText(bodyX + 1, bodyY + 1, line2, PixelRenderer::BLACK, bodyFs);
    PixelRenderer::drawPixelText(bodyX, bodyY, line2, PixelRenderer::YELLOW, bodyFs);
    PixelRenderer::drawPixelText(hintX, hintY, line3, PixelRenderer::GRAY, 1.0f);
}

void TerrariumScene::drawVisitRecallConfirm() {
    LGFX_Sprite& canvas = Hal::ins().canvas();
    int x = 38;
    int y = 48;
    int w = 164;
    int h = 54;
    PixelRenderer::fillRect(x, y, w, h, PixelRenderer::BLACK);
    PixelRenderer::fillRect(x + 2, y + 2, w - 4, h - 4, 0x2104);

    float titleFs = PixelRenderer::getContentFontScale();
    PixelRenderer::applyTextStyle(titleFs);
    int titleW = canvas.textWidth(UiStrings::VISIT_RECALL_CONFIRM);
    int titleX = x + (w - titleW) / 2;
    PixelRenderer::drawPixelText(titleX, y + 12, UiStrings::VISIT_RECALL_CONFIRM, PixelRenderer::WHITE, titleFs);

    PixelRenderer::applyTextStyle(1.0f);
    int navW = canvas.textWidth(UiStrings::VISIT_RECALL_NAV);
    int navX = x + (w - navW) / 2;
    PixelRenderer::drawPixelText(navX, y + 34, UiStrings::VISIT_RECALL_NAV, PixelRenderer::YELLOW, 1.0f);
}

void TerrariumScene::drawDeathScreen() {
    PixelRenderer::fillRect(40, 40, 160, 55, PixelRenderer::BLACK);
    PixelRenderer::fillRect(45, 45, 150, 45, PixelRenderer::RED);
    PixelRenderer::drawPixelText(55, 55, UiStrings::DIED, PixelRenderer::WHITE, 3);
    PixelRenderer::drawPixelText(55, 80, UiStrings::HOLD_AB_RESET, PixelRenderer::WHITE, 1);
}

// 倾斜检测：每帧读取 IMU，防抖 200ms 后触发
void TerrariumScene::updateTilt() {
    float ax, ay, az;
    Hal::ins().getAccel(ax, ay, az);

    // 垂直方向过滤：竖屏/举起时忽略，避免甲虫乱跑
    if (fabsf(ay) > 1.5f) {
        static uint32_t lastLog = 0;
        if (Hal::ins().millis() - lastLog >= 1000) {
            Serial.printf("[Tilt] ignore, ay=%.2f (vertical)\n", ay);
            lastLog = Hal::ins().millis();
        }
        return;
    }

    // 摇晃优先：剧烈摇晃时跳过倾斜，避免冲突
    float mag = Hal::ins().getAccelMagnitude();
    if (mag > 2.0f) {
        static uint32_t lastShakeLog = 0;
        if (Hal::ins().millis() - lastShakeLog >= 500) {
            Serial.printf("[Tilt] ignore, mag=%.2f (shake)\n", mag);
            lastShakeLog = Hal::ins().millis();
        }
        return;
    }

    TiltDir newDir = TiltDir::NONE;
    if (ax > TILT_THRESHOLD_G) newDir = TiltDir::LEFT;
    else if (ax < -TILT_THRESHOLD_G) newDir = TiltDir::RIGHT;

    const char* highSideStr = (newDir == TiltDir::LEFT) ? "RIGHT" :
                              (newDir == TiltDir::RIGHT) ? "LEFT" : "NONE";
    const char* lowSideStr  = (newDir == TiltDir::LEFT) ? "LEFT" :
                              (newDir == TiltDir::RIGHT) ? "RIGHT" : "NONE";

    // 方向变化时打印：明确显示高低方向
    if (newDir != pendingTiltDir) {
        Serial.printf("[Tilt] ax=%.2f pending low=%s/%d -> low=%s/%d  (high=%s)\n",
                      ax,
                      (pendingTiltDir == TiltDir::LEFT) ? "LEFT" :
                      (pendingTiltDir == TiltDir::RIGHT) ? "RIGHT" : "NONE",
                      (int)pendingTiltDir,
                      lowSideStr, (int)newDir, highSideStr);
        pendingTiltDir = newDir;
        tiltStableMs = Hal::ins().millis();
    }

    if (newDir != activeTiltDir &&
        Hal::ins().millis() - tiltStableMs >= TILT_DEBOUNCE_MS) {
        activeTiltDir = newDir;
        Serial.printf("[Tilt] STABLE low=%s/%d high=%s (stable %dms)\n",
                      lowSideStr, (int)activeTiltDir, highSideStr, TILT_DEBOUNCE_MS);
        if (activeTiltDir != TiltDir::NONE) {
            onTilt(activeTiltDir, mag);
        } else {
            // 恢复水平：如果正在爬坡，停下来休息
            if (adultState == AdultState::CLIMB) {
                Serial.println("[Tilt] level, stop climb -> IDLE");
                adultState = AdultState::IDLE;
                stateTimer = 0;
                setIdleDuration();
            }
        }
    }
}

// 倾斜触发：
// - 小角度：转身朝高处，然后缓慢爬行或原地不动
// - 大角度：先向低处滑落一小段，再转身朝高处爬行
// dir 表示低处方向（设备哪一侧向下）
void TerrariumScene::onTilt(TiltDir dir, float magnitude) {
    mind.onTilted(Hal::ins().millis());
    const char* lowSideStr  = (dir == TiltDir::LEFT) ? "LEFT" : "RIGHT";
    const char* highSideStr = (dir == TiltDir::LEFT) ? "RIGHT" : "LEFT";
    Serial.printf("[Tilt] onTilt low=%s high=%s mag=%.2f adultState=%d bugX=%d\n",
                  lowSideStr, highSideStr, magnitude, (int)adultState, bugX);

    if (GameEngine::ins().isVisitHost()) {
        applyTiltToVisitor(dir, magnitude);
    }

    if (adultState == AdultState::EAT) {
        Serial.println("[Tilt] ignored: eating");
        return;
    }

    if (adultState == AdultState::REST && magnitude <= TILT_SLIDE_THRESHOLD_G) {
        Serial.println("[Tilt] ignored: resting");
        return;
    }

    // threaten（威吓）和 REST（夜间休息）期间只响应大角度滑落，
    // 小角度爬行被忽略；大角度滑落会唤醒休息中的甲虫。
    if (pokeThreatenEndMs != 0 && Hal::ins().millis() < pokeThreatenEndMs) {
        if (magnitude <= TILT_SLIDE_THRESHOLD_G) {
            Serial.println("[Tilt] ignored: threatening");
            return;
        }
    }

    // 低处在左 => 高处在右；低处在右 => 高处在左
    bool lowIsLeft = (dir == TiltDir::LEFT);
    tiltHighSideIsRight = !lowIsLeft;

    // 高处目标位置
    int highTarget = bugX + (tiltHighSideIsRight ? TILT_CLIMB_DISTANCE : -TILT_CLIMB_DISTANCE);
    if (highTarget < MIN_X) highTarget = MIN_X;
    if (highTarget > MAX_X) highTarget = MAX_X;
    climbTargetX = highTarget;

    if (magnitude > TILT_SLIDE_THRESHOLD_G) {
        // 大角度：先向低处滑落；若从夜间休息中滑落，设置随机清醒冷却
        if (adultState == AdultState::REST) {
            restResumeAllowedMs = Hal::ins().millis() + random(REST_WAKEUP_COOLDOWN_MIN_MS,
                                                               REST_WAKEUP_COOLDOWN_MAX_MS + 1);
        }
        int lowTarget = bugX + (lowIsLeft ? -TILT_SLIDE_DISTANCE : +TILT_SLIDE_DISTANCE);
        if (lowTarget < MIN_X) lowTarget = MIN_X;
        if (lowTarget > MAX_X) lowTarget = MAX_X;
        slideTargetX = lowTarget;

        Serial.printf("[Tilt] slide first -> lowTarget=%d then climb=%d\n",
                      slideTargetX, climbTargetX);

        bool needFaceRight = !tiltHighSideIsRight;  // 面向低处
        if (needFaceRight != faceRight) {
            startTurn(needFaceRight, true);
            slideAfterTurn = true;
        } else {
            adultState = AdultState::SLIDE;
            stateTimer = 0;
            stateDuration = 0;
        }
    } else {
        // 小角度：转身朝高处，然后爬行或不动
        Serial.printf("[Tilt] climb/highSide -> target=%d\n", climbTargetX);

        bool needFaceRight = tiltHighSideIsRight;
        if (needFaceRight != faceRight) {
            startTurn(needFaceRight, true);
            climbAfterTurn = true;
        } else {
            startClimbOrIdle();
        }
    }
}

// 小角度倾斜：优先向高处缓慢爬行，偶尔原地休息
void TerrariumScene::startClimbOrIdle() {
    if (random(100) < 45) {
        Serial.printf("[Tilt] start CLIMB -> target=%d\n", climbTargetX);
        adultState = AdultState::CLIMB;
        stateTimer = 0;
        stateDuration = 0;
    } else {
        Serial.println("[Tilt] stay IDLE at high side");
        adultState = AdultState::IDLE;
        stateTimer = 0;
        setIdleDuration();
    }
}

// 幼虫被戳：三段身体收缩 2 像素
void TerrariumScene::drawLarvaPoked(int x, int y, uint8_t palette) {
    int shake = ((Hal::ins().millis() / 70) % 2) ? 2 : -2;
    drawLarva(x + shake, y, palette);
}

int TerrariumScene::getPokeTargetX() const {
    return pokeFingerTargetVisitor && visitor.active ? visitor.x : bugX;
}

int TerrariumScene::getPokeTargetY() const {
    if (pokeFingerTargetVisitor && visitor.active) {
        return GROUND_Y - 20;
    }

    Bug& bug = GameEngine::ins().getBug();
    switch (bug.getStage()) {
        case Stage::EGG:
            return GROUND_Y - 7;
        case Stage::PUPA:
            return 95;
        case Stage::LARVA:
            return GROUND_Y - 5;
        case Stage::JUVENILE:
        case Stage::ADULT:
        default:
            return adultFootY() - 20;
    }
}

void TerrariumScene::startPokeFeedback(uint32_t now, uint32_t durationMs, bool wasPoked, bool targetVisitor) {
    pokeFingerStartMs = now;
    pokeReactionEndMs = now + durationMs;
    pokeReactionWasPoked = wasPoked;
    pokeFingerTargetVisitor = targetVisitor;
    if (wasPoked) {
        pokeFingerFromRight = random(2) == 0;
        pokeFingerFrameIndex = random(ActionAssets::FINGER_FRAME_COUNT);
        pokeFingerYOffset = (int8_t)random(-3, 4);
    }
}

void TerrariumScene::showTerrariumToast(const char* msg, uint32_t durationMs) {
    if (!msg) return;
    strncpy(terrariumToastText, msg, sizeof(terrariumToastText) - 1);
    terrariumToastText[sizeof(terrariumToastText) - 1] = '\0';
    terrariumToastUntilMs = Hal::ins().millis() + durationMs;
}

void TerrariumScene::drawTerrariumToast() {
    uint32_t now = Hal::ins().millis();
    if (terrariumToastText[0] == '\0' || now >= terrariumToastUntilMs) return;

    float fs = PixelRenderer::getContentFontScale();
    if (fs < 1.5f) fs = 1.5f;
    LGFX_Sprite& canvas = Hal::ins().canvas();
    PixelRenderer::applyTextStyle(fs);
    int textW = canvas.textWidth(terrariumToastText);
    int boxW = textW + 18;
    if (boxW > Hal::DISPLAY_W - 16) boxW = Hal::DISPLAY_W - 16;
    int boxH = (int)(18 * fs);
    int boxX = (Hal::DISPLAY_W - boxW) / 2;
    int boxY = 52;
    PixelRenderer::fillRect(boxX, boxY, boxW, boxH, PixelRenderer::rgb565(18, 18, 24));
    canvas.drawRect(boxX, boxY, boxW, boxH, PixelRenderer::YELLOW);
    PixelRenderer::drawPixelText(boxX + (boxW - textW) / 2,
                                 boxY + (boxH - (int)(8 * fs)) / 2,
                                 terrariumToastText,
                                 PixelRenderer::WHITE, fs);
}

void TerrariumScene::drawPokeAction() {
    if (!pokeReactionWasPoked || pokeReactionEndMs == 0) return;

    uint32_t now = Hal::ins().millis();
    if (now >= pokeReactionEndMs) return;

    uint32_t duration = pokeReactionEndMs - pokeFingerStartMs;
    if (duration == 0) return;

    uint32_t elapsed = now - pokeFingerStartMs;
    if (elapsed > duration) elapsed = duration;

    // 先伸入，再回收，形成一次明确的“戳”位移动画。
    const uint32_t pushDuration = (duration * 45) / 100;
    const int travel = 14;
    int advance;
    if (elapsed <= pushDuration || pushDuration == 0) {
        advance = (pushDuration == 0) ? travel : (int)((elapsed * travel) / pushDuration);
    } else {
        uint32_t retractElapsed = elapsed - pushDuration;
        uint32_t retractDuration = duration - pushDuration;
        advance = travel - (int)((retractElapsed * travel) / retractDuration);
    }
    if (advance < 0) advance = 0;
    if (advance > travel) advance = travel;

    uint8_t frameIndex = pokeFingerFrameIndex;
    if (frameIndex >= ActionAssets::FINGER_FRAME_COUNT) frameIndex = 0;
    uint16_t offset = pgm_read_word(&ActionAssets::FINGER_FRAMES[frameIndex].offset);
    uint16_t length = pgm_read_word(&ActionAssets::FINGER_FRAMES[frameIndex].length);

    static constexpr int CONTACT_OFFSET_X = 18;

    const int targetX = getPokeTargetX() + (pokeFingerFromRight ? CONTACT_OFFSET_X : -CONTACT_OFFSET_X);
    const int targetY = getPokeTargetY() + pokeFingerYOffset;
    uint8_t frameW = pgm_read_byte(&ActionAssets::FINGER_FRAMES[frameIndex].width);
    uint8_t frameH = pgm_read_byte(&ActionAssets::FINGER_FRAMES[frameIndex].height);
    uint8_t tipX = pgm_read_byte(&ActionAssets::FINGER_FRAMES[frameIndex].tipX);
    uint8_t tipY = pgm_read_byte(&ActionAssets::FINGER_FRAMES[frameIndex].tipY);
    if (pokeFingerFromRight) {
        tipX = frameW - 1 - tipX;
    }

    int x;
    if (pokeFingerFromRight) {
        x = targetX - tipX + travel - advance;
    } else {
        x = targetX - tipX - travel + advance;
    }
    int y = targetY - tipY;

    PixelRenderer::drawRgb565Rle(x, y,
                                 frameW,
                                 frameH,
                                 ActionAssets::FINGER_RLE,
                                 offset, length,
                                 pokeFingerFromRight);
}

// 戳冷却提示：甲虫上方闪烁小点
void TerrariumScene::drawPokeCooldownHint(int x, int y) {
    uint16_t c = ((Hal::ins().millis() / 100) % 2) ? PixelRenderer::GRAY : PixelRenderer::BLACK;
    int hintY = y - 16;
    if (hintY < 0) hintY = 0;
    PixelRenderer::fillRect(x - 2, hintY, 4, 4, c);
}
