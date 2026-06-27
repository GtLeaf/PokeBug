void ExploreScene::render() {
    PixelRenderer::fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::rgb565(0, 0, 0));

    switch (state) {
        case State::EXPLORING:
            drawExploring();
            break;
        case State::EVENT_REVEAL:
            drawExploring(revealShakeOffset(Hal::ins().millis()));
            break;
        case State::EVENT_POPUP:
        case State::FINAL_SUMMARY:
            drawPopup();
            break;
        case State::RELEASE_PROMPT:
            drawReleasePrompt();
            break;
        case State::NPC_PROMPT:
            drawNpcPrompt();
            break;
        default:
            break;
    }
}

void ExploreScene::drawSkyAndGround(int shakeX) {
    LGFX_Sprite& canvas = Hal::ins().canvas();
    ExploreAssets::IndexedImage bg = ExploreAssets::background(GameEngine::ins().getExploreLocation(),
                                                               GameEngine::ins().getTimeOfDay());
    if (bg.indices && bg.palette) {
        PixelRenderer::drawIndexed8(shakeX, 0,
                                    ExploreAssets::FRAME_W,
                                    ExploreAssets::FRAME_H,
                                    bg.indices,
                                    bg.palette);
        return;
    }

    // 天空渐变：上蓝下浅
    for (int y = 0; y < PROCEDURAL_GROUND_Y; y++) {
        uint8_t r = 0, g = (uint8_t)(40 + y / 3), b = (uint8_t)(60 + y / 4);
        canvas.drawFastHLine(0, y, Hal::DISPLAY_W, PixelRenderer::rgb565(r, g, b));
    }
    // 太阳
    canvas.fillCircle(Hal::DISPLAY_W - 24, 18, 6, PixelRenderer::YELLOW);
    // 草地
    PixelRenderer::fillRect(0, PROCEDURAL_GROUND_Y, Hal::DISPLAY_W, Hal::DISPLAY_H - PROCEDURAL_GROUND_Y,
                            PixelRenderer::rgb565(0, 80, 0));
    // 草丛装饰
    for (int x = 10; x < Hal::DISPLAY_W; x += 45) {
        PixelRenderer::fillRect(x, PROCEDURAL_GROUND_Y - 6, 4, 10, PixelRenderer::rgb565(0, 100, 0));
        PixelRenderer::fillRect(x + 6, PROCEDURAL_GROUND_Y - 4, 4, 8, PixelRenderer::rgb565(0, 110, 0));
    }
}

void ExploreScene::drawExploring(int shakeX, bool showProgressText) {
    drawSkyAndGround(shakeX);
    if (!showProgressText) return;

    LGFX_Sprite& canvas = Hal::ins().canvas();
    float fs = PixelRenderer::getContentFontScale();
    const char* text = UiStrings::EXPLORE_IN_PROGRESS;
    PixelRenderer::applyTextStyle(fs);
    int tw = canvas.textWidth(text);
    int th = (int)(8 * fs);
    int padX = 8;
    int padY = 4;
    int boxW = tw + padX * 2;
    int boxH = th + padY * 2;
    int boxX = (Hal::DISPLAY_W - boxW) / 2 + shakeX;
    int boxY = (Hal::DISPLAY_H - boxH) / 2;
    fillRectAlpha(boxX, boxY, boxW, boxH, PixelRenderer::rgb565(24, 24, 24), 170);
    canvas.drawRect(boxX, boxY, boxW, boxH, PixelRenderer::WHITE);
    PixelRenderer::drawPixelText(boxX + padX, boxY + padY, text, PixelRenderer::WHITE, fs);
}

void ExploreScene::drawPopup() {
    LGFX_Sprite& canvas = Hal::ins().canvas();
    float fs = PixelRenderer::getContentFontScale();
    drawExploring(0, false);
    fillRectAlpha(20, 30, Hal::DISPLAY_W - 40, Hal::DISPLAY_H - 60, PixelRenderer::rgb565(24, 24, 24), 190);
    canvas.drawRect(20, 30, Hal::DISPLAY_W - 40, Hal::DISPLAY_H - 60, PixelRenderer::WHITE);

    int cx = Hal::DISPLAY_W / 2;
    PixelRenderer::applyTextStyle(fs);
    char roundBuf[20];
    if (state == State::FINAL_SUMMARY) {
        snprintf(roundBuf, sizeof(roundBuf), "%s", GameEngine::ins().getTimeOfDayShortName());
    } else {
        snprintf(roundBuf, sizeof(roundBuf), UiStrings::EXPLORE_ROUND_FMT, currentRound);
    }
    int tw = canvas.textWidth(roundBuf);
    PixelRenderer::drawPixelText(cx - tw / 2, 34, roundBuf, PixelRenderer::YELLOW, fs);

    tw = canvas.textWidth(resultLine1);
    PixelRenderer::drawPixelText(cx - tw / 2, 50, resultLine1, PixelRenderer::WHITE, fs);
    if (resultLine2[0]) {
        tw = canvas.textWidth(resultLine2);
        PixelRenderer::drawPixelText(cx - tw / 2, 50 + (int)(10 * fs), resultLine2, PixelRenderer::CYAN, fs);
    }
    const char* nav = (state == State::FINAL_SUMMARY) ? UiStrings::EXPLORE_RETURN
                                                      : UiStrings::EXPLORE_NEXT_RETURN;
    tw = canvas.textWidth(nav);
    PixelRenderer::drawPixelText(cx - tw / 2, Hal::DISPLAY_H - 44, nav, PixelRenderer::GRAY, fs);
}

void ExploreScene::drawReleasePrompt() {
    LGFX_Sprite& canvas = Hal::ins().canvas();
    float fs = PixelRenderer::getContentFontScale();
    drawExploring(0, false);
    fillRectAlpha(14, 26, Hal::DISPLAY_W - 28, Hal::DISPLAY_H - 52, PixelRenderer::rgb565(24, 24, 24), 195);
    canvas.drawRect(14, 26, Hal::DISPLAY_W - 28, Hal::DISPLAY_H - 52, PixelRenderer::WHITE);

    int cx = Hal::DISPLAY_W / 2;
    PixelRenderer::applyTextStyle(fs);
    int tw = canvas.textWidth(resultLine1);
    PixelRenderer::drawPixelText(cx - tw / 2, 36, resultLine1, PixelRenderer::YELLOW, fs);

    tw = canvas.textWidth(UiStrings::EXPLORE_RELEASE_CONFIRM);
    PixelRenderer::drawPixelText(cx - tw / 2, 56, UiStrings::EXPLORE_RELEASE_CONFIRM, PixelRenderer::WHITE, fs);

    const char* detail = resultLine2[0] ? resultLine2 : UiStrings::EXPLORE_RELEASE_EGG;
    tw = canvas.textWidth(detail);
    PixelRenderer::drawPixelText(cx - tw / 2, 74, detail, PixelRenderer::CYAN, fs);

    PixelRenderer::drawPixelText(cx - 58, Hal::DISPLAY_H - 36, UiStrings::EXPLORE_RELEASE_ACTION, PixelRenderer::GREEN, fs);
    PixelRenderer::drawPixelText(cx + 18, Hal::DISPLAY_H - 36, UiStrings::EXPLORE_LEAVE, PixelRenderer::GRAY, fs);
}

void ExploreScene::drawNpcPrompt() {
    LGFX_Sprite& canvas = Hal::ins().canvas();
    float fs = PixelRenderer::getContentFontScale();
    drawExploring(0, false);
    fillRectAlpha(16, 24, Hal::DISPLAY_W - 32, Hal::DISPLAY_H - 48, PixelRenderer::rgb565(24, 24, 24), 190);
    canvas.drawRect(16, 24, Hal::DISPLAY_W - 32, Hal::DISPLAY_H - 48, PixelRenderer::WHITE);

    int cx = Hal::DISPLAY_W / 2;
    PixelRenderer::applyTextStyle(fs);
    char buf[48];
    snprintf(buf, sizeof(buf), UiStrings::EXPLORE_NPC_TITLE_FMT,
             NpcData::tierName(npc.tier), npcName);
    int tw = canvas.textWidth(buf);
    PixelRenderer::drawPixelText(cx - tw / 2, 36, buf, PixelRenderer::YELLOW, fs);
    snprintf(buf, sizeof(buf), UiStrings::EXPLORE_BEETLE_LABEL, npcBugName);
    tw = canvas.textWidth(buf);
    PixelRenderer::drawPixelText(cx - tw / 2, 52, buf, PixelRenderer::WHITE, fs);
    tw = canvas.textWidth(npcMeetLine);
    PixelRenderer::drawPixelText(cx - tw / 2, 70, npcMeetLine, PixelRenderer::CYAN, fs);

    PixelRenderer::drawPixelText(cx - 58, Hal::DISPLAY_H - 36, UiStrings::EXPLORE_ACCEPT, PixelRenderer::GREEN, fs);
    PixelRenderer::drawPixelText(cx + 20, Hal::DISPLAY_H - 36, UiStrings::EXPLORE_LEAVE, PixelRenderer::GRAY, fs);
}

void ExploreScene::drawResult() {
    drawPopup();
}
