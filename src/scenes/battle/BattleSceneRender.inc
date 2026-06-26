void BattleScene::render() {
    switch (state) {
        case State::CONNECTING:
            drawConnecting();
            break;
        case State::DONE:
            drawResult();
            break;
        default:
            drawBattleField();
            break;
    }
}

void BattleScene::drawConnecting() {
    PixelRenderer::fillRect(0, 0, 240, 135, PixelRenderer::BLACK);
    PixelRenderer::drawPixelText(50, 50, UiStrings::BATTLE_SEARCHING, PixelRenderer::WHITE, 2);

    uint32_t elapsed = Hal::ins().millis() - stateStartMs;
    int dots = (elapsed / 500) % 4;
    char buf[8];
    snprintf(buf, sizeof(buf), "%.*s", dots, UiStrings::ELLIPSIS);
    PixelRenderer::drawPixelText(100, 80, buf, PixelRenderer::WHITE, 1);
}

void BattleScene::drawCombatantSprite(const Combatant& combatant, int centerX, int groundY,
                                      bool faceRight, int8_t shakeX, int8_t shakeY,
                                      bool attacking, bool critical) {
    const HerculesAdultSprites::RleFrame* frames = HerculesAdultSprites::WALK_FRAMES;
    const uint16_t* data = HerculesAdultSprites::WALK_RLE;
    uint8_t frameCount = HerculesAdultSprites::WALK_FRAME_COUNT;
    uint32_t durationMs = 1;
    uint32_t elapsed = Hal::ins().millis() - stateStartMs;

    if (attacking && elapsed < ATTACK_MS / 2) {
        frames = HerculesAdultSprites::ATTACK_DOWN_FRAMES;
        data = HerculesAdultSprites::ATTACK_DOWN_RLE;
        frameCount = HerculesAdultSprites::ATTACK_DOWN_FRAME_COUNT;
        durationMs = ATTACK_MS / 2;
        if (critical) {
            centerX += faceRight ? -3 : 3;
        }
    } else if (attacking) {
        frames = HerculesAdultSprites::ATTACK_UP_FRAMES;
        data = HerculesAdultSprites::ATTACK_UP_RLE;
        frameCount = HerculesAdultSprites::ATTACK_UP_FRAME_COUNT;
        uint32_t attackElapsed = elapsed - ATTACK_MS / 2;
        durationMs = ATTACK_MS / 2;
        uint32_t phase = attackElapsed < durationMs ? attackElapsed : durationMs;
        if (critical) {
            int lunge = (int)(phase * 14 / durationMs);
            centerX += faceRight ? lunge : -lunge;
        }
        elapsed = attackElapsed;
    }

    uint8_t frameIndex = 0;
    if (frameCount > 1) {
        if (elapsed > durationMs) elapsed = durationMs;
        frameIndex = (uint8_t)((elapsed * frameCount) / (durationMs + 1));
        if (frameIndex >= frameCount) frameIndex = frameCount - 1;
    }

    uint16_t offset = pgm_read_word(&frames[frameIndex].offset);
    uint16_t length = pgm_read_word(&frames[frameIndex].length);
    uint8_t w = pgm_read_byte(&frames[frameIndex].width);
    uint8_t h = pgm_read_byte(&frames[frameIndex].height);
    float scale = 1.0f;
    int drawW = (int)(w * scale);
    int drawH = (int)(h * scale);
    int drawX = centerX - drawW / 2 + shakeX;
    int drawY = groundY - drawH + shakeY;

    PixelRenderer::fillRect(centerX - 27 + shakeX, groundY + 2 + shakeY, 54, 3,
                            PixelRenderer::rgb565(24, 24, 28));
    uint16_t paletteColor = battlePaletteColor(combatant.palette);
    PixelRenderer::drawRgb565RleMappedScaled(drawX, drawY, w, h,
                                             data, offset, length, scale,
                                             HerculesAdultSprites::PALETTE_KEY, paletteColor,
                                             HerculesAdultSprites::PALETTE_KEY, paletteColor,
                                             HerculesAdultSprites::PALETTE_KEY, paletteColor,
                                             !faceRight);
}

int BattleScene::tempoScore(const Combatant& combatant) const {
    int score = TEMPO_BASE_SCORE + (int)combatant.spd * TEMPO_SPD_SLOPE;
    return score < 1 ? 1 : score;
}

float BattleScene::tempoProgress(bool forMe) const {
    return forMe ? myGauge : enemyGauge;
}

void BattleScene::drawTempoBar() {
    PixelRenderer::drawPixelText(TEMPO_LABEL_X, TEMPO_BAR_Y - 3,
                                 UiStrings::BATTLE_TEMPO, PixelRenderer::GRAY, 1);
    PixelRenderer::fillRect(TEMPO_BAR_X, TEMPO_BAR_Y, TEMPO_BAR_W, TEMPO_BAR_H,
                            PixelRenderer::rgb565(30, 30, 36));
    PixelRenderer::fillRect(TEMPO_BAR_X, TEMPO_BAR_Y + 2, TEMPO_BAR_W, 1,
                            PixelRenderer::rgb565(72, 72, 82));

    if (isRhythmWindowActive()) {
        const RhythmWindow* window = currentRhythmWindow();
        int zoneX = TEMPO_BAR_X + window->startPx;
        int perfectHalfPx = window->widthPx / 5;
        if (perfectHalfPx < 2) perfectHalfPx = 2;
        int centerX = zoneX + window->widthPx / 2;
        uint16_t barBase = PixelRenderer::rgb565(30, 30, 36);
        uint16_t zoneColor = mixBattleRgb565(barBase, PixelRenderer::ORANGE, 0.45f);
        uint16_t perfectColor = mixBattleRgb565(barBase, PixelRenderer::YELLOW, 0.58f);
        PixelRenderer::fillRect(zoneX, TEMPO_BAR_Y - 1, window->widthPx,
                                TEMPO_BAR_H + 2, zoneColor);
        PixelRenderer::fillRect(centerX - perfectHalfPx, TEMPO_BAR_Y - 1,
                                perfectHalfPx * 2 + 1, TEMPO_BAR_H + 2,
                                perfectColor);
    }

    PixelRenderer::fillRect(TEMPO_BAR_X + TEMPO_BAR_W, TEMPO_BAR_Y - 1, 2,
                            TEMPO_BAR_H + 2, PixelRenderer::CREAM);

    auto drawMarker = [&](bool mine, float progress, int yOffset) {
        if (progress < 0.0f) progress = 0.0f;
        if (progress > 1.0f) progress = 1.0f;
        int x = TEMPO_BAR_X + (int)(progress * TEMPO_BAR_W);
        int y = TEMPO_ICON_Y + yOffset;
        uint16_t color = mine ? PixelRenderer::CYAN : PixelRenderer::rgb565(230, 90, 100);
        PixelRenderer::fillRect(x - 3, y - 2, 7, 5, color);
        PixelRenderer::fillRect(x - 2, y - 1, 5, 3, brightenBattleRgb565(color, 1.25f));
    };

    drawMarker(true, tempoProgress(true), -2);
    drawMarker(false, tempoProgress(false), 4);
}

void BattleScene::drawBattleField() {
    PixelRenderer::fillRect(0, 0, 240, 135, PixelRenderer::rgb565(20, 20, 30));

    // 计算受击晃动偏移
    uint32_t now = Hal::ins().millis();
    auto computeShake = [](uint32_t nowMs, uint32_t endMs, int8_t amp, int8_t& outX, int8_t& outY) {
        if (nowMs >= endMs) { outX = 0; outY = 0; return; }
        outX = ((nowMs / 50) % 2) ? amp : -amp;
        outY = ((nowMs / 40) % 2) ? amp : -amp;
    };
    int8_t meOffX = 0, meOffY = 0;
    int8_t enemyOffX = 0, enemyOffY = 0;
    computeShake(now, meShakeEndMs, SHAKE_AMP, meOffX, meOffY);
    computeShake(now, enemyShakeEndMs, SHAKE_AMP, enemyOffX, enemyOffY);

    // 标题
    char buf[32];
    snprintf(buf, sizeof(buf), "%s %d", UiStrings::BATTLE_ROUND, roundNum);
    PixelRenderer::drawPixelText(90, 5, buf, PixelRenderer::WHITE, 1);

    bool meAttacking = (state == State::ATTACK_ONE || state == State::ATTACK_TWO) &&
                       isCurrentAttackByMe();
    bool enemyAttacking = (state == State::ATTACK_ONE || state == State::ATTACK_TWO) &&
                          !isCurrentAttackByMe();
    bool currentAttackCritical = (state == State::ATTACK_ONE || state == State::ATTACK_TWO) &&
                                 isCurrentAttackCritical();

    // 我方（左侧）
    drawCombatantSprite(me, 58, 62, true, meOffX, meOffY,
                        meAttacking, meAttacking && currentAttackCritical);
    snprintf(buf, sizeof(buf), "%s:%d/%d", UiStrings::BATTLE_HP, me.hp, me.maxHp);
    PixelRenderer::drawPixelText(10, 60, buf, PixelRenderer::WHITE, 1);
    PixelRenderer::drawProgressBar(10, 72, 80, 6, (float)me.hp / me.maxHp,
                                   me.hp > me.maxHp / 2 ? PixelRenderer::GREEN : PixelRenderer::RED,
                                   PixelRenderer::GRAY);
    snprintf(buf, sizeof(buf), "%s:%d", UiStrings::BATTLE_MOT, me.mot);
    PixelRenderer::drawPixelText(10, 82, buf, PixelRenderer::WHITE, 1);

    // 敌方（右侧）
    drawCombatantSprite(enemy, 182, 62, false, enemyOffX, enemyOffY,
                        enemyAttacking, enemyAttacking && currentAttackCritical);
    snprintf(buf, sizeof(buf), "%s:%d/%d", UiStrings::BATTLE_HP, enemy.hp, enemy.maxHp);
    PixelRenderer::drawPixelText(150, 60, buf, PixelRenderer::WHITE, 1);
    PixelRenderer::drawProgressBar(150, 72, 80, 6, (float)enemy.hp / enemy.maxHp,
                                   enemy.hp > enemy.maxHp / 2 ? PixelRenderer::GREEN : PixelRenderer::RED,
                                   PixelRenderer::GRAY);

    // 中间状态提示
    const char* msg = "";
    uint16_t msgColor = PixelRenderer::YELLOW;
    float msgScale = 1;
    bool msgBold = false;
    int msgX = 80;
    int msgY = 100;
    if (now < rhythmFeedbackUntilMs && rhythmFeedback != RhythmFeedback::NONE) {
        switch (rhythmFeedback) {
            case RhythmFeedback::PERFECT:
                msg = UiStrings::BATTLE_FEEDBACK_MOT_PLUS_PLUS;
                msgColor = PixelRenderer::MAGENTA;
                break;
            case RhythmFeedback::GREAT:
                msg = UiStrings::BATTLE_FEEDBACK_MOT_PLUS;
                msgColor = PixelRenderer::CYAN;
                break;
            case RhythmFeedback::MISS:
                msg = UiStrings::BATTLE_FEEDBACK_MISS;
                msgColor = PixelRenderer::GRAY;
                break;
            case RhythmFeedback::NONE:
            default:
                break;
        }
    } else {
        switch (state) {
            case State::SYNCING: msg = UiStrings::BATTLE_STATE_SYNC; break;
            case State::GAUGE_FILLING:
                msg = isRhythmWindowActive() ? UiStrings::BATTLE_STATE_TIMING
                                             : UiStrings::BATTLE_STATE_CHARGE;
                break;
            case State::ATTACK_ONE:
            case State::ATTACK_TWO:
                msg = isCurrentAttackByMe() ? UiStrings::BATTLE_STATE_ATTACK
                                            : UiStrings::BATTLE_STATE_FOE_ATTACK;
                msgX = isCurrentAttackByMe() ? 34 : 154;
                msgY = 24;
                break;
            case State::ROUND_END:
                msg = myAttackDodged ? UiStrings::BATTLE_FEEDBACK_MISS :
                      (enemyAttackDodged ? UiStrings::BATTLE_STATE_DODGE :
                       (myCrit ? UiStrings::BATTLE_STATE_CRIT :
                        (enemyCrit ? UiStrings::BATTLE_STATE_OUCH : "")));
                break;
            case State::TIMEOUT:
                msg = UiStrings::BATTLE_STATE_TIME_OUT;
                msgColor = PixelRenderer::RED;
                msgScale = 2;
                msgBold = true;
                msgX = 64;
                msgY = 94;
                break;
            default: break;
        }
    }
    PixelRenderer::drawPixelText(msgX, msgY, msg, msgColor, msgScale);
    if (msgBold) {
        PixelRenderer::drawPixelText(msgX + 1, msgY, msg, msgColor, msgScale);
    }

    // 暴击闪屏（仅一帧）
    if (flashThisFrame) {
        PixelRenderer::fillRect(0, 0, 240, 135, PixelRenderer::WHITE);
        flashThisFrame = false;
    }

    drawTempoBar();
}

void BattleScene::drawResult() {
    PixelRenderer::fillRect(0, 0, 240, 135, PixelRenderer::BLACK);
    const char* resultText;
    uint16_t color;
    if (noOpponent) {
        resultText = UiStrings::BATTLE_RESULT_NO_FOE;
        color = PixelRenderer::YELLOW;
    } else if (localWin) {
        resultText = UiStrings::BATTLE_RESULT_WIN;
        color = PixelRenderer::GREEN;
    } else {
        resultText = UiStrings::BATTLE_RESULT_LOSE;
        color = PixelRenderer::RED;
    }
    PixelRenderer::drawPixelText(noOpponent ? 70 : 90, 45, resultText, color, 3);
    PixelRenderer::drawPixelText(70, 90, UiStrings::BATTLE_PRESS_A, PixelRenderer::WHITE, 1);
}
