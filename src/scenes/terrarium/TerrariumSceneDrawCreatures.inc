void TerrariumScene::drawBackground() {
    if (GameEngine::ins().getMainSceneBg() == GameEngine::BG_BEGINNER) {
        PixelRenderer::drawIndexed8(0, 0,
                                    MainSceneAssets::GIRL_ROOM_FULL_W,
                                    MainSceneAssets::GIRL_ROOM_FULL_H,
                                    MainSceneAssets::GIRL_ROOM_FULL_INDEX,
                                    MainSceneAssets::GIRL_ROOM_FULL_PALETTE);
        return;
    }
    if (GameEngine::ins().getMainSceneBg() == GameEngine::BG_CHILD_ROOM) {
        PixelRenderer::drawIndexed8(0, 0,
                                    MainSceneAssets::BOY_ROOM_FULL_W,
                                    MainSceneAssets::BOY_ROOM_FULL_H,
                                    MainSceneAssets::BOY_ROOM_FULL_INDEX,
                                    MainSceneAssets::BOY_ROOM_FULL_PALETTE);
        return;
    }
    if (GameEngine::ins().getMainSceneBg() == GameEngine::BG_ENTOMOLOGIST) {
        if (GameEngine::ins().isNight()) {
            PixelRenderer::drawIndexed8(0, 0,
                                        MainSceneAssets::ENTOMOLOGIST_ROOM_NIGHT_FULL_W,
                                        MainSceneAssets::ENTOMOLOGIST_ROOM_NIGHT_FULL_H,
                                        MainSceneAssets::ENTOMOLOGIST_ROOM_NIGHT_FULL_INDEX,
                                        MainSceneAssets::ENTOMOLOGIST_ROOM_NIGHT_FULL_PALETTE);
        } else {
            PixelRenderer::drawIndexed8(0, 0,
                                        MainSceneAssets::ENTOMOLOGIST_ROOM_DAY_FULL_W,
                                        MainSceneAssets::ENTOMOLOGIST_ROOM_DAY_FULL_H,
                                        MainSceneAssets::ENTOMOLOGIST_ROOM_DAY_FULL_INDEX,
                                        MainSceneAssets::ENTOMOLOGIST_ROOM_DAY_FULL_PALETTE);
        }
        return;
    }
    if (GameEngine::ins().getMainSceneBg() == GameEngine::BG_SCHOOL) {
        if (GameEngine::ins().isNight()) {
            PixelRenderer::drawIndexed8(0, 0,
                                        MainSceneAssets::SCHOOL_NIGHT_FULL_W,
                                        MainSceneAssets::SCHOOL_NIGHT_FULL_H,
                                        MainSceneAssets::SCHOOL_NIGHT_FULL_INDEX,
                                        MainSceneAssets::SCHOOL_NIGHT_FULL_PALETTE);
        } else {
            PixelRenderer::drawIndexed8(0, 0,
                                        MainSceneAssets::SCHOOL_DAY_FULL_W,
                                        MainSceneAssets::SCHOOL_DAY_FULL_H,
                                        MainSceneAssets::SCHOOL_DAY_FULL_INDEX,
                                        MainSceneAssets::SCHOOL_DAY_FULL_PALETTE);
        }
        return;
    }

    PixelRenderer::drawIndexed8(0, 0,
                                MainSceneAssets::ROOM_FULL_W,
                                MainSceneAssets::ROOM_FULL_H,
                                MainSceneAssets::ROOM_FULL_INDEX,
                                MainSceneAssets::ROOM_FULL_PALETTE);
}

void TerrariumScene::drawBug() {
    Bug& bug = GameEngine::ins().getBug();
    uint8_t pal = bug.getPaletteId();

    // 戳反应覆盖绘制
    if (pokeReactionEndMs != 0 && Hal::ins().millis() < pokeReactionEndMs) {
        if (pokeReactionWasPoked) {
            if (bug.getStage() == Stage::EGG) {
                int shake = ((Hal::ins().millis() / 50) % 2) ? 2 : -2;
                drawEgg(bugX + shake, GROUND_Y - 7, pal);
                return;
            }
            if (bug.getStage() == Stage::LARVA) {
                drawLarvaPoked(bugX, GROUND_Y - 5, pal);
                return;
            }
            if (bug.getStage() == Stage::PUPA) {
                int shake = ((Hal::ins().millis() / 100) % 2) ? PUPA_POKE_SHAKE_PX : -PUPA_POKE_SHAKE_PX;
                drawPupa(Hal::DISPLAY_W / 2 + shake,
                         Hal::DISPLAY_H - PUPA_BOTTOM_MARGIN_PX - HerculesPupaSprites::FRAME_H / 2,
                         pal);
                return;
            }
            // 成虫：fall through 到 drawAdult，其内部会处理 poke 反应（威吓姿态）
        } else {
            // 冷却中：在甲虫上方画闪烁提示
            int baseY = 95;
            if (bug.getStage() == Stage::EGG) baseY = GROUND_Y - 7;
            else if (bug.getStage() == Stage::LARVA) baseY = GROUND_Y - 5;
            else if (bug.getStage() == Stage::ADULT) baseY = GROUND_Y;
            drawPokeCooldownHint(bugX, baseY);
        }
    }

    switch (bug.getStage()) {
        case Stage::EGG:
            drawEgg(bugX, GROUND_Y - 7, pal);
            break;
        case Stage::LARVA:
            drawLarva(bugX, GROUND_Y - 5, pal);
            break;
        case Stage::PUPA:
            drawPupa(Hal::DISPLAY_W / 2,
                     Hal::DISPLAY_H - PUPA_BOTTOM_MARGIN_PX - HerculesPupaSprites::FRAME_H / 2,
                     pal);
            break;
        case Stage::JUVENILE:
            drawAdult(bugX, GROUND_Y, pal);
            break;
        case Stage::ADULT:
        default:
            drawAdult(bugX, GROUND_Y, pal);
            break;
    }
}

float TerrariumScene::visitorAdultScale() const {
    if (visitor.siz < 6) return 0.9f;
    if (visitor.siz < 14) return 1.0f;
    if (visitor.siz < 22) return 1.1f;
    return 1.2f;
}

float TerrariumScene::visitorAdultDrawScale() const {
    float scale = visitorAdultScale();
    return scale < 1.0f ? 1.0f : scale;
}

uint16_t TerrariumScene::visitorAdultColor() const {
    if ((visitor.palette & 0x80) != 0) {
        uint8_t temper = visitor.palette & 0x07;
        if (temper > (uint8_t)Temperament::SPIRIT) temper = (uint8_t)Temperament::BRUTE;
        uint8_t bucket = (visitor.palette >> 3) & 0x03;
        float depth = (float)bucket / 3.0f;
        return adultDepthColor((Temperament)temper, depth);
    }

    switch (visitor.palette & 0x03) {
        case 1: return 0x0400;
        case 2: return 0xFE00;
        case 3: return 0xE71C;
        case 0:
        default:
            return adultHueMain(Temperament::BRUTE);
    }
}

void TerrariumScene::drawVisitorAdult(int x, int y) {
    const HerculesAdultSprites::RleFrame* frames = HerculesAdultSprites::WALK_FRAMES;
    const uint16_t* data = HerculesAdultSprites::WALK_RLE;
    uint8_t frameCount = HerculesAdultSprites::WALK_FRAME_COUNT;
    uint8_t frameIndex = visitor.falling ? 0 : (animFrame / 14) % frameCount;
    bool flipSprite = !visitor.faceRight;

    if (visitor.actor.state == AdultState::THREATEN) {
        frames = HerculesAdultSprites::THREATEN_FRAMES;
        data = HerculesAdultSprites::THREATEN_RLE;
        frameCount = HerculesAdultSprites::THREATEN_FRAME_COUNT;
        uint32_t elapsed = Hal::ins().millis() - visitor.actor.threatenStartMs;
        if (elapsed < THREATEN_PLAY_MS) {
            frameIndex = (elapsed * frameCount) / THREATEN_PLAY_MS;
            if (frameIndex >= frameCount) frameIndex = frameCount - 1;
        } else if (elapsed < THREATEN_PLAY_MS + THREATEN_HOLD_MS) {
            frameIndex = frameCount - 1;
        } else {
            uint32_t returnElapsed = elapsed - THREATEN_PLAY_MS - THREATEN_HOLD_MS;
            uint8_t reverseStep = (returnElapsed * frameCount) / THREATEN_RETURN_MS;
            frameIndex = reverseStep >= frameCount ? 0 : (frameCount - 1 - reverseStep);
        }
        flipSprite = !visitor.faceRight;
    } else if (visitor.actor.state == AdultState::ATTACK_DOWN) {
        frames = HerculesAdultSprites::ATTACK_DOWN_FRAMES;
        data = HerculesAdultSprites::ATTACK_DOWN_RLE;
        frameCount = HerculesAdultSprites::ATTACK_DOWN_FRAME_COUNT;
        uint32_t elapsed = Hal::ins().millis() - visitorToyChargeStartMs;
        uint32_t duration = visitorToyChargeDurationMs == 0 ? 1 : visitorToyChargeDurationMs;
        frameIndex = (elapsed * frameCount) / duration;
        if (frameIndex >= frameCount) frameIndex = frameCount - 1;
        flipSprite = !visitor.faceRight;
    } else if (visitor.actor.state == AdultState::ATTACK_UP) {
        uint32_t elapsed = Hal::ins().millis() - visitorToyAttackStartMs;
        frames = HerculesAdultSprites::ATTACK_UP_FRAMES;
        data = HerculesAdultSprites::ATTACK_UP_RLE;
        frameCount = HerculesAdultSprites::ATTACK_UP_FRAME_COUNT;
        frameIndex = (elapsed * frameCount) / TOY_ATTACK_UP_MS;
        if (frameIndex >= frameCount) frameIndex = frameCount - 1;
        flipSprite = !visitor.faceRight;
    } else if (visitor.actor.state == AdultState::EAT) {
        frames = HerculesAdultSprites::EAT_FRAMES;
        data = HerculesAdultSprites::EAT_RLE;
        frameCount = HerculesAdultSprites::EAT_FRAME_COUNT;
        frameIndex = (visitor.actor.stateTimer / EAT_FRAME_INTERVAL_MIN) % frameCount;
        flipSprite = !visitor.faceRight;
    } else if (visitor.actor.state == AdultState::TURN) {
        frames = HerculesAdultSprites::TURN_FRAMES;
        data = HerculesAdultSprites::TURN_RLE;
        frameCount = HerculesAdultSprites::TURN_FRAME_COUNT;
        frameIndex = visitor.actor.turnFrameIndex;
        if (frameIndex >= frameCount) frameIndex = frameCount - 1;
        flipSprite = false;
    } else if (visitor.actor.state == AdultState::REST) {
        uint32_t getDownFrames = HerculesAdultSprites::SLEEP_GETDOWN_FRAME_COUNT *
                                 REST_GETDOWN_FRAME_INTERVAL;
        if (visitor.actor.stateTimer < getDownFrames) {
            frames = HerculesAdultSprites::SLEEP_GETDOWN_FRAMES;
            data = HerculesAdultSprites::SLEEP_GETDOWN_RLE;
            frameCount = HerculesAdultSprites::SLEEP_GETDOWN_FRAME_COUNT;
            frameIndex = visitor.actor.stateTimer / REST_GETDOWN_FRAME_INTERVAL;
            if (frameIndex >= frameCount) frameIndex = frameCount - 1;
        } else {
            frames = HerculesAdultSprites::SLEEP_BREATH_FRAMES;
            data = HerculesAdultSprites::SLEEP_BREATH_RLE;
            frameCount = HerculesAdultSprites::SLEEP_BREATH_FRAME_COUNT;
            frameIndex = ((visitor.actor.stateTimer - getDownFrames) / REST_BREATH_FRAME_INTERVAL) % frameCount;
        }
        flipSprite = !visitor.faceRight;
    } else if (visitor.actor.state == AdultState::SLIDE) {
        frameIndex = 0;
        flipSprite = !visitor.faceRight;
    } else if (visitor.actor.state == AdultState::CLIMB) {
        frameIndex = (animFrame / 20) % frameCount;
        flipSprite = !visitor.faceRight;
    } else if (visitor.actor.state == AdultState::IDLE) {
        frameIndex = 0;
    }

    uint16_t offset = pgm_read_word(&frames[frameIndex].offset);
    uint16_t length = pgm_read_word(&frames[frameIndex].length);
    uint8_t frameW = pgm_read_byte(&frames[frameIndex].width);
    uint8_t frameH = pgm_read_byte(&frames[frameIndex].height);
    float scale = visitorAdultDrawScale();
    uint16_t color = visitorAdultColor();
    int drawX = (int)(x - (frameW * scale) / 2.0f);
    int drawY = (int)(y - frameH * scale);
    if (visitor.faceRight && visitor.actor.state != AdultState::TURN) {
        drawX -= 2;
    }

    if (drawCachedMappedRleFrame(drawX, drawY,
                                 frameW, frameH,
                                 data, offset, length,
                                 scale,
                                 HerculesAdultSprites::PALETTE_KEY, color,
                                 HerculesAdultSprites::PALETTE_KEY, color,
                                 HerculesAdultSprites::PALETTE_KEY, color,
                                 flipSprite)) {
        return;
    }
    PixelRenderer::drawRgb565RleMappedScaled(drawX, drawY,
                                             frameW, frameH,
                                             data, offset, length,
                                             scale,
                                             HerculesAdultSprites::PALETTE_KEY, color,
                                             HerculesAdultSprites::PALETTE_KEY, color,
                                             HerculesAdultSprites::PALETTE_KEY, color,
                                             flipSprite);
}

void TerrariumScene::drawVisitor() {
    if (!visitor.active) return;
    int drawY = visitor.y;
    drawVisitorAdult(visitor.x, drawY);
}

void TerrariumScene::drawEgg(int x, int y, uint8_t palette) {
    (void)palette;
    Bug& bug = GameEngine::ins().getBug();
    float progress = bug.getStageProgress(GameEngine::ins().getGameNow());
    uint8_t frameIndex = (uint8_t)(progress * HerculesEggSprites::FRAME_COUNT);
    if (frameIndex >= HerculesEggSprites::FRAME_COUNT) {
        frameIndex = HerculesEggSprites::FRAME_COUNT - 1;
    }

    uint16_t offset = pgm_read_word(&HerculesEggSprites::FRAMES[frameIndex].offset);
    uint16_t length = pgm_read_word(&HerculesEggSprites::FRAMES[frameIndex].length);
    uint8_t w = pgm_read_byte(&HerculesEggSprites::FRAMES[frameIndex].width);
    uint8_t h = pgm_read_byte(&HerculesEggSprites::FRAMES[frameIndex].height);
    if (drawCachedRleFrame(x - w / 2, y + 9 - h,
                           w, h,
                           HerculesEggSprites::RLE, offset, length,
                           1.0f, false)) {
        return;
    }
    PixelRenderer::drawRgb565Rle(x - w / 2, y + 9 - h, w, h,
                                 HerculesEggSprites::RLE, offset, length);
}

void TerrariumScene::drawLarva(int x, int y, uint8_t palette) {
    (void)palette;
    Bug& bug = GameEngine::ins().getBug();
    uint8_t ageIndex = bug.getLarvaAgeIndex(GameEngine::ins().getGameNow());

    uint32_t elapsed = Hal::ins().millis() - larvaStateStartMs;
    bool showWalk = elapsed >= larvaWalkStartDelayMs(bug.getHunger()) &&
                    abs(larvaTargetX - bugX) > 1;
    const auto* frames = HerculesLarvaSprites::IDLE_FRAMES;
    const uint16_t* rle = HerculesLarvaSprites::IDLE_RLE;
    uint8_t frameIndex = ageIndex;
    if (larvaState == LarvaState::EAT) {
        frames = HerculesLarvaSprites::EAT_FRAMES;
        rle = HerculesLarvaSprites::EAT_RLE;
        uint8_t eatFrame = (elapsed / LARVA_EAT_FRAME_MS) % HerculesLarvaSprites::EAT_FRAME_COUNT;
        frameIndex = ageIndex * HerculesLarvaSprites::EAT_FRAME_COUNT + eatFrame;
    } else if (larvaState == LarvaState::SLEEP) {
        frames = HerculesLarvaSprites::SLEEP_FRAMES;
        rle = HerculesLarvaSprites::SLEEP_RLE;
        uint8_t sleepStep = (elapsed / LARVA_SLEEP_FRAME_MS) %
                            (HerculesLarvaSprites::SLEEP_FRAME_COUNT * 2 - 2);
        uint8_t sleepFrame = sleepStep < HerculesLarvaSprites::SLEEP_FRAME_COUNT
                                 ? sleepStep
                                 : (HerculesLarvaSprites::SLEEP_FRAME_COUNT * 2 - 2 - sleepStep);
        frameIndex = ageIndex * HerculesLarvaSprites::SLEEP_FRAME_COUNT + sleepFrame;
    } else if (showWalk) {
        frames = HerculesLarvaSprites::WALK_FRAMES;
        rle = HerculesLarvaSprites::WALK_RLE;
        uint8_t walkFrame = (elapsed / LARVA_EAT_FRAME_MS) % HerculesLarvaSprites::WALK_FRAME_COUNT;
        frameIndex = ageIndex * HerculesLarvaSprites::WALK_FRAME_COUNT + walkFrame;
    }

    uint16_t offset = pgm_read_word(&frames[frameIndex].offset);
    uint16_t length = pgm_read_word(&frames[frameIndex].length);
    uint8_t w = pgm_read_byte(&frames[frameIndex].width);
    uint8_t h = pgm_read_byte(&frames[frameIndex].height);
    int drawX = x - w / 2;
    int drawY = y + 5 - h;
    if (!drawCachedRleFrame(drawX, drawY,
                            w, h,
                            rle, offset, length,
                            1.0f, !larvaFaceRight)) {
        PixelRenderer::drawRgb565Rle(drawX, drawY, w, h, rle, offset, length,
                                     !larvaFaceRight);
    }
}

void TerrariumScene::drawPupa(int x, int y, uint8_t palette) {
    (void)palette;
    Bug& bug = GameEngine::ins().getBug();
    float progress = bug.getStageProgress(GameEngine::ins().getGameNow());
    uint8_t frameIndex = (uint8_t)(progress * HerculesPupaSprites::FRAME_COUNT);
    if (frameIndex >= HerculesPupaSprites::FRAME_COUNT) {
        frameIndex = HerculesPupaSprites::FRAME_COUNT - 1;
    }

    uint16_t offset = pgm_read_word(&HerculesPupaSprites::FRAMES[frameIndex].offset);
    uint16_t length = pgm_read_word(&HerculesPupaSprites::FRAMES[frameIndex].length);
    uint8_t w = pgm_read_byte(&HerculesPupaSprites::FRAMES[frameIndex].width);
    uint8_t h = pgm_read_byte(&HerculesPupaSprites::FRAMES[frameIndex].height);
    if (drawCachedRleFrame(x - w / 2, y - h / 2,
                           w, h,
                           HerculesPupaSprites::RLE, offset, length,
                           1.0f, false)) {
        return;
    }
    PixelRenderer::drawRgb565Rle(x - w / 2, y - h / 2, w, h,
                                 HerculesPupaSprites::RLE, offset, length);
}

void TerrariumScene::drawAdult(int x, int y, uint8_t palette) {
    (void)palette;

    Bug& bug = GameEngine::ins().getBug();
    const HerculesAdultSprites::RleFrame* frames = HerculesAdultSprites::WALK_FRAMES;
    const uint16_t* data = HerculesAdultSprites::WALK_RLE;
    uint8_t frameCount = HerculesAdultSprites::WALK_FRAME_COUNT;
    uint8_t frameIndex = (animFrame / 10) % frameCount;
    bool flipSprite = !faceRight;

    if (bug.isDead()) {
        // 死亡叠层：使用 reset 第 0 帧
        frames = HerculesAdultSprites::RESET_FRAMES;
        data = HerculesAdultSprites::RESET_RLE;
        frameCount = HerculesAdultSprites::RESET_FRAME_COUNT;
        frameIndex = 0;
        flipSprite = !faceRight;
    } else if (pokeThreatenEndMs != 0 && pokeReactionWasPoked && bug.getStage() == Stage::ADULT) {
        // 戳反应：威吓正播、保持，结束前反向播放回站姿，避免直接跳回 walk。
        frames = HerculesAdultSprites::THREATEN_FRAMES;
        data = HerculesAdultSprites::THREATEN_RLE;
        frameCount = HerculesAdultSprites::THREATEN_FRAME_COUNT;
        uint32_t elapsed = Hal::ins().millis() - pokeReactionStartMs;
        if (elapsed < THREATEN_PLAY_MS) {
            frameIndex = (elapsed * frameCount) / THREATEN_PLAY_MS;
            if (frameIndex >= frameCount) frameIndex = frameCount - 1;
        } else if (elapsed < THREATEN_PLAY_MS + THREATEN_HOLD_MS) {
            frameIndex = frameCount - 1;
        } else {
            uint32_t returnElapsed = elapsed - THREATEN_PLAY_MS - THREATEN_HOLD_MS;
            uint8_t reverseStep = (returnElapsed * frameCount) / THREATEN_RETURN_MS;
            frameIndex = reverseStep >= frameCount ? 0 : (frameCount - 1 - reverseStep);
        }
        flipSprite = !faceRight;
    } else if (toyCharging && isMobileBeetleStage(bug.getStage())) {
        frames = HerculesAdultSprites::ATTACK_DOWN_FRAMES;
        data = HerculesAdultSprites::ATTACK_DOWN_RLE;
        frameCount = HerculesAdultSprites::ATTACK_DOWN_FRAME_COUNT;
        uint32_t elapsed = Hal::ins().millis() - toyChargeStartMs;
        uint32_t duration = toyChargeDurationMs == 0 ? 1 : toyChargeDurationMs;
        frameIndex = (elapsed * frameCount) / duration;
        if (frameIndex >= frameCount) frameIndex = frameCount - 1;
        flipSprite = !faceRight;
    } else if (toyAttackStartMs != 0 &&
               Hal::ins().millis() - toyAttackStartMs < TOY_ATTACK_UP_MS &&
               isMobileBeetleStage(bug.getStage())) {
        uint32_t elapsed = Hal::ins().millis() - toyAttackStartMs;
        frames = HerculesAdultSprites::ATTACK_UP_FRAMES;
        data = HerculesAdultSprites::ATTACK_UP_RLE;
        frameCount = HerculesAdultSprites::ATTACK_UP_FRAME_COUNT;
        frameIndex = (elapsed * frameCount) / TOY_ATTACK_UP_MS;
        if (frameIndex >= frameCount) frameIndex = frameCount - 1;
        flipSprite = !faceRight;
    } else if (adultState == AdultState::REST) {
        // 夜间休息：先播放一次入睡 getDown，之后只循环 breath。
        uint32_t getDownFrames = HerculesAdultSprites::SLEEP_GETDOWN_FRAME_COUNT *
                                 REST_GETDOWN_FRAME_INTERVAL;
        if (stateTimer < getDownFrames) {
            frames = HerculesAdultSprites::SLEEP_GETDOWN_FRAMES;
            data = HerculesAdultSprites::SLEEP_GETDOWN_RLE;
            frameCount = HerculesAdultSprites::SLEEP_GETDOWN_FRAME_COUNT;
            frameIndex = stateTimer / REST_GETDOWN_FRAME_INTERVAL;
            if (frameIndex >= frameCount) frameIndex = frameCount - 1;
        } else {
            frames = HerculesAdultSprites::SLEEP_BREATH_FRAMES;
            data = HerculesAdultSprites::SLEEP_BREATH_RLE;
            frameCount = HerculesAdultSprites::SLEEP_BREATH_FRAME_COUNT;
            frameIndex = ((stateTimer - getDownFrames) / REST_BREATH_FRAME_INTERVAL) % frameCount;
        }
        flipSprite = !faceRight;
    } else if (adultState == AdultState::EAT) {
        // 进食动画
        frames = HerculesAdultSprites::EAT_FRAMES;
        data = HerculesAdultSprites::EAT_RLE;
        frameCount = HerculesAdultSprites::EAT_FRAME_COUNT;
        uint8_t interval = eatFrameInterval == 0 ? EAT_FRAME_INTERVAL_MIN : eatFrameInterval;
        frameIndex = (stateTimer / interval) % frameCount;
        flipSprite = !faceRight;
    } else if (adultState == AdultState::TURN) {
        // 转身过渡每次随机取一张中间姿态；同一次转身中保持不变。
        frames = HerculesAdultSprites::TURN_FRAMES;
        data = HerculesAdultSprites::TURN_RLE;
        frameCount = HerculesAdultSprites::TURN_FRAME_COUNT;
        frameIndex = turnFrameIndex;
        if (frameIndex >= frameCount) frameIndex = frameCount - 1;
        flipSprite = false;
    } else if (adultState == AdultState::SLIDE) {
        // 滑落：保持当前姿势滑下去，不播放 walk 动画
        frameIndex = 0;
        flipSprite = !faceRight;
    } else if (adultState == AdultState::CLIMB) {
        // 爬坡：使用 walk 帧，很慢播放
        frameIndex = (animFrame / 20) % frameCount;
        flipSprite = !faceRight;
    } else if (adultState == AdultState::IDLE) {
        // 静止时停在 walk 第 0 帧（站立姿态）
        frameIndex = 0;
        flipSprite = !faceRight;
    }

    uint16_t offset = pgm_read_word(&frames[frameIndex].offset);
    uint16_t length = pgm_read_word(&frames[frameIndex].length);
    uint8_t frameW = pgm_read_byte(&frames[frameIndex].width);
    uint8_t frameH = pgm_read_byte(&frames[frameIndex].height);
    float adultScale = bug.getAdultScale();
    uint16_t adultHue = adultDepthColor(bug.getTemperament(), bug.getAdultDepth());
    int drawX = (int)(x - (frameW * adultScale) / 2.0f);
    int drawY = (int)(y - frameH * adultScale);
    // y 是脚/底部参考点，让精灵底部对齐 y
    if (drawCachedMappedRleFrame(drawX, drawY,
                                 frameW, frameH,
                                 data, offset, length,
                                 adultScale,
                                 HerculesAdultSprites::PALETTE_KEY, adultHue,
                                 HerculesAdultSprites::PALETTE_KEY, adultHue,
                                 HerculesAdultSprites::PALETTE_KEY, adultHue,
                                 flipSprite)) {
        return;
    }
    PixelRenderer::drawRgb565RleMappedScaled(drawX, drawY,
                                             frameW, frameH,
                                             data, offset, length,
                                             adultScale,
                                             HerculesAdultSprites::PALETTE_KEY, adultHue,
                                             HerculesAdultSprites::PALETTE_KEY, adultHue,
                                             HerculesAdultSprites::PALETTE_KEY, adultHue,
                                             flipSprite);
}

// 判断成虫是否想去进食：饥饿或纯粹嘴馋
