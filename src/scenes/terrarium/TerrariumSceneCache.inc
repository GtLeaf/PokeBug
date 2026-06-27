bool TerrariumScene::ensureCacheSprite(LGFX_Sprite& sprite, int w, int h) {
    if (w <= 0 || h <= 0) return false;
    if (sprite.getBuffer() && sprite.width() == w && sprite.height() == h) {
        return true;
    }

    sprite.deleteSprite();
    sprite.setPsram(psramFound());
    sprite.setColorDepth(16);
    sprite.setSwapBytes(true);
    if (!sprite.createSprite(w, h)) {
        return false;
    }
    return true;
}

void TerrariumScene::invalidateRenderCaches() {
    staticLayerCache.valid = false;
    hudCache.valid = false;
}

bool TerrariumScene::staticLayerMatches() const {
    if (!staticLayerCache.valid || !staticLayerCache.sprite.getBuffer()) return false;

    const Bug& bug = GameEngine::ins().getBug();
    return staticLayerCache.bg == GameEngine::ins().getMainSceneBg() &&
           staticLayerCache.night == GameEngine::ins().isNight() &&
           staticLayerCache.stage == bug.getStage();
}

void TerrariumScene::rebuildStaticLayer() {
    if (!ensureCacheSprite(staticLayerCache.sprite, Hal::DISPLAY_W, Hal::DISPLAY_H)) {
        staticLayerCache.valid = false;
        return;
    }

    PixelRenderer::bind(&staticLayerCache.sprite);
    drawBackground();
    PixelRenderer::bind(&Hal::ins().canvas());

    const Bug& bug = GameEngine::ins().getBug();
    staticLayerCache.valid = true;
    staticLayerCache.bg = GameEngine::ins().getMainSceneBg();
    staticLayerCache.night = GameEngine::ins().isNight();
    staticLayerCache.stage = bug.getStage();
}

void TerrariumScene::drawStaticLayer() {
    if (!staticLayerMatches()) {
        rebuildStaticLayer();
    }

    if (staticLayerCache.valid && staticLayerCache.sprite.getBuffer()) {
        staticLayerCache.sprite.pushSprite(&Hal::ins().canvas(), 0, 0);
        return;
    }

    drawBackground();
}

bool TerrariumScene::hudCacheMatches(const char* clockBuf) const {
    if (!hudCache.valid || !hudCache.sprite.getBuffer()) return false;
    const Bug& bug = GameEngine::ins().getBug();
    bool visitHost = GameEngine::ins().isVisitHost();
    uint8_t fontBucket = PixelRenderer::getContentFontScale() < 1.65f ? 0 : 1;
    uint16_t remainingSec = 0;
    uint16_t durationSec = 0;
    uint8_t guestHunger = 0;
    uint8_t guestMot = 0;
    if (visitHost) {
        remainingSec = (uint16_t)((GameEngine::ins().getVisitRemainingMs() + 999UL) / 1000UL);
        durationSec = (uint16_t)((GameEngine::ins().getVisitDurationMs() + 999UL) / 1000UL);
        const VisitBugSnapshot& guest = GameEngine::ins().getVisitRemoteBug();
        guestHunger = guest.hunger;
        guestMot = guest.motivation;
    }

    return hudCache.visitHost == visitHost &&
           hudCache.fontBucket == fontBucket &&
           hudCache.weatherDayMod == (GameEngine::ins().getCurrentGameDay() % 3) &&
           hudCache.hostHunger == bug.getHunger() &&
           hudCache.hostMot == bug.getMot() &&
           hudCache.guestHunger == guestHunger &&
           hudCache.guestMot == guestMot &&
           hudCache.remainingSec == remainingSec &&
           hudCache.durationSec == durationSec &&
           strcmp(hudCache.clock, clockBuf) == 0;
}

void TerrariumScene::rebuildHudCache(const char* clockBuf) {
    if (!ensureCacheSprite(hudCache.sprite, Hal::DISPLAY_W, HUD_CACHE_H)) {
        hudCache.valid = false;
        return;
    }

    hudCache.sprite.fillSprite(CACHE_TRANSPARENT);
    PixelRenderer::bind(&hudCache.sprite);
    drawStatusBar();
    PixelRenderer::bind(&Hal::ins().canvas());

    const Bug& bug = GameEngine::ins().getBug();
    bool visitHost = GameEngine::ins().isVisitHost();
    hudCache.valid = true;
    hudCache.visitHost = visitHost;
    hudCache.fontBucket = PixelRenderer::getContentFontScale() < 1.65f ? 0 : 1;
    hudCache.weatherDayMod = GameEngine::ins().getCurrentGameDay() % 3;
    hudCache.hostHunger = bug.getHunger();
    hudCache.hostMot = bug.getMot();
    hudCache.remainingSec = 0;
    hudCache.durationSec = 0;
    hudCache.guestHunger = 0;
    hudCache.guestMot = 0;
    if (visitHost) {
        hudCache.remainingSec = (uint16_t)((GameEngine::ins().getVisitRemainingMs() + 999UL) / 1000UL);
        hudCache.durationSec = (uint16_t)((GameEngine::ins().getVisitDurationMs() + 999UL) / 1000UL);
        const VisitBugSnapshot& guest = GameEngine::ins().getVisitRemoteBug();
        hudCache.guestHunger = guest.hunger;
        hudCache.guestMot = guest.motivation;
    }
    strncpy(hudCache.clock, clockBuf, sizeof(hudCache.clock) - 1);
    hudCache.clock[sizeof(hudCache.clock) - 1] = '\0';
}

void TerrariumScene::drawCachedStatusBar() {
    if (GameEngine::ins().isVisitGuest()) return;

    char clockBuf[8];
    GameEngine::ins().getExploreClockText(clockBuf, sizeof(clockBuf));
    if (!hudCacheMatches(clockBuf)) {
        rebuildHudCache(clockBuf);
    }

    if (hudCache.valid && hudCache.sprite.getBuffer()) {
        hudCache.sprite.pushSprite(&Hal::ins().canvas(), 0, 0, CACHE_TRANSPARENT);
        return;
    }
    drawStatusBar();
}

bool TerrariumScene::frameCacheMatches(const BeetleFrameCache& cache,
                                       int cacheW, int cacheH,
                                       uintptr_t dataId, uint16_t offset, uint16_t length,
                                       uint8_t frameW, uint8_t frameH, uint16_t scaleQ,
                                       bool mapped,
                                       uint16_t keyMain, uint16_t targetMain,
                                       uint16_t keyShadow, uint16_t targetShadow,
                                       uint16_t keyMarking, uint16_t targetMarking,
                                       bool flipSprite) const {
    return cache.valid && cache.sprite.getBuffer() &&
           cache.sprite.width() == cacheW &&
           cache.sprite.height() == cacheH &&
           cache.dataId == dataId &&
           cache.offset == offset &&
           cache.length == length &&
           cache.frameW == frameW &&
           cache.frameH == frameH &&
           cache.scaleQ == scaleQ &&
           cache.mapped == mapped &&
           cache.keyMain == keyMain &&
           cache.targetMain == targetMain &&
           cache.keyShadow == keyShadow &&
           cache.targetShadow == targetShadow &&
           cache.keyMarking == keyMarking &&
           cache.targetMarking == targetMarking &&
           cache.flip == flipSprite;
}

TerrariumScene::BeetleFrameCache* TerrariumScene::selectFrameCacheSlot() {
    BeetleFrameCache* oldest = &frameCache[0];
    for (uint8_t i = 0; i < FRAME_CACHE_SLOTS; ++i) {
        if (!frameCache[i].valid || !frameCache[i].sprite.getBuffer()) {
            return &frameCache[i];
        }
        if (frameCache[i].lastUsed < oldest->lastUsed) {
            oldest = &frameCache[i];
        }
    }
    oldest->valid = false;
    return oldest;
}

bool TerrariumScene::drawCachedRleFrame(int drawX, int drawY,
                                        uint8_t frameW, uint8_t frameH,
                                        const uint16_t* data, uint16_t offset,
                                        uint16_t length, float scale,
                                        bool flipSprite) {
    return drawCachedFrame(drawX, drawY,
                           frameW, frameH,
                           data, offset, length,
                           scale,
                           false,
                           0, 0,
                           0, 0,
                           0, 0,
                           flipSprite);
}

bool TerrariumScene::drawCachedMappedRleFrame(int drawX, int drawY,
                                              uint8_t frameW, uint8_t frameH,
                                              const uint16_t* data, uint16_t offset,
                                              uint16_t length, float scale,
                                              uint16_t keyMain, uint16_t targetMain,
                                              uint16_t keyShadow, uint16_t targetShadow,
                                              uint16_t keyMarking, uint16_t targetMarking,
                                              bool flipSprite) {
    return drawCachedFrame(drawX, drawY,
                           frameW, frameH,
                           data, offset, length,
                           scale,
                           true,
                           keyMain, targetMain,
                           keyShadow, targetShadow,
                           keyMarking, targetMarking,
                           flipSprite);
}

bool TerrariumScene::drawCachedFrame(int drawX, int drawY,
                                     uint8_t frameW, uint8_t frameH,
                                     const uint16_t* data, uint16_t offset,
                                     uint16_t length, float scale,
                                     bool mapped,
                                     uint16_t keyMain, uint16_t targetMain,
                                     uint16_t keyShadow, uint16_t targetShadow,
                                     uint16_t keyMarking, uint16_t targetMarking,
                                     bool flipSprite) {
    if (!data || frameW == 0 || frameH == 0 || scale <= 0.0f) return false;

    int cacheW = (int)ceilf(frameW * scale) + 2;
    int cacheH = (int)ceilf(frameH * scale) + 2;
    if (cacheW < 3) cacheW = 3;
    if (cacheH < 3) cacheH = 3;
    uint16_t scaleQ = (uint16_t)(scale * 1000.0f + 0.5f);
    uintptr_t dataId = (uintptr_t)data;

    for (uint8_t i = 0; i < FRAME_CACHE_SLOTS; ++i) {
        if (frameCacheMatches(frameCache[i],
                              cacheW, cacheH,
                              dataId, offset, length,
                              frameW, frameH, scaleQ,
                              mapped,
                              keyMain, targetMain,
                              keyShadow, targetShadow,
                              keyMarking, targetMarking,
                              flipSprite)) {
            frameCache[i].lastUsed = ++frameCacheClock;
            frameCache[i].sprite.pushSprite(&Hal::ins().canvas(),
                                            drawX - 1, drawY - 1,
                                            CACHE_TRANSPARENT);
            return true;
        }
    }

    BeetleFrameCache* cache = selectFrameCacheSlot();
    if (!ensureCacheSprite(cache->sprite, cacheW, cacheH)) {
        cache->valid = false;
        return false;
    }
    cache->sprite.fillSprite(CACHE_TRANSPARENT);
    PixelRenderer::bind(&cache->sprite);
    if (mapped) {
        PixelRenderer::drawRgb565RleMappedScaled(1, 1,
                                                 frameW,
                                                 frameH,
                                                 data, offset, length,
                                                 scale,
                                                 keyMain, targetMain,
                                                 keyShadow, targetShadow,
                                                 keyMarking, targetMarking,
                                                 flipSprite);
    } else {
        PixelRenderer::drawRgb565RleScaled(1, 1,
                                           frameW,
                                           frameH,
                                           data, offset, length,
                                           scale,
                                           flipSprite);
    }
    PixelRenderer::bind(&Hal::ins().canvas());

    cache->valid = true;
    cache->lastUsed = ++frameCacheClock;
    cache->dataId = dataId;
    cache->offset = offset;
    cache->length = length;
    cache->frameW = frameW;
    cache->frameH = frameH;
    cache->scaleQ = scaleQ;
    cache->mapped = mapped;
    cache->keyMain = keyMain;
    cache->targetMain = targetMain;
    cache->keyShadow = keyShadow;
    cache->targetShadow = targetShadow;
    cache->keyMarking = keyMarking;
    cache->targetMarking = targetMarking;
    cache->flip = flipSprite;
    cache->sprite.pushSprite(&Hal::ins().canvas(), drawX - 1, drawY - 1, CACHE_TRANSPARENT);
    return true;
}
