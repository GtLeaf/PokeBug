void TerrariumScene::clearLarvaSubstrate() {
    memset(substrateLevels, 0, sizeof(substrateLevels));
    memset(substrateBites, 0, sizeof(substrateBites));
    for (uint8_t i = 0; i < SUBSTRATE_DROP_SLOTS; ++i) {
        substrateDrops[i].active = false;
    }
    substrateFadeActive = false;
    substrateFadeStartMs = 0;
    substrateLastUpdateMs = Hal::ins().millis();
    larvaLastSubstrateBiteMs = 0;
}

void TerrariumScene::startLarvaSubstrateFade(uint32_t nowMs) {
    if (!hasLarvaSubstrate()) return;
    substrateFadeActive = true;
    substrateFadeStartMs = nowMs;
    for (uint8_t i = 0; i < SUBSTRATE_DROP_SLOTS; ++i) {
        substrateDrops[i].active = false;
    }
    larvaTargetX = bugX;
    larvaState = LarvaState::IDLE;
    larvaStateStartMs = nowMs;
    larvaStateDurationMs = random(LARVA_IDLE_MIN_MS, LARVA_IDLE_MAX_MS + 1);
    larvaNextStepMs = nowMs + LARVA_WALK_START_MS;
    Serial.println("[Terrarium] Larva substrate fade started");
}

uint8_t TerrariumScene::substrateFrameIndex(uint8_t level) const {
    if (level == 0) return 0;
    if (level > SubstrateAssets::FRAME_COUNT) return SubstrateAssets::FRAME_COUNT - 1;
    return level - 1;
}

uint8_t TerrariumScene::substrateBiteCapacity(uint8_t level) const {
    if (level >= 3) return SUBSTRATE_LEVEL3_BITES;
    if (level == 2) return SUBSTRATE_LEVEL2_BITES;
    if (level == 1) return SUBSTRATE_LEVEL1_BITES;
    return 0;
}

uint8_t TerrariumScene::substrateLevelForBites(uint8_t bites) const {
    if (bites == 0) return 0;
    if (bites <= SUBSTRATE_LEVEL1_BITES) return 1;
    if (bites <= SUBSTRATE_LEVEL2_BITES) return 2;
    return 3;
}

void TerrariumScene::setSubstrateBites(uint8_t slot, uint8_t bites) {
    if (slot >= SUBSTRATE_SLOT_COUNT) return;
    substrateBites[slot] = bites;
    substrateLevels[slot] = substrateLevelForBites(bites);
}

void TerrariumScene::clearSubstrateSlot(uint8_t slot) {
    if (slot >= SUBSTRATE_SLOT_COUNT) return;
    substrateLevels[slot] = 0;
    substrateBites[slot] = 0;
}

uint8_t TerrariumScene::substrateFrameWidth(uint8_t level) const {
    uint8_t idx = substrateFrameIndex(level);
    return pgm_read_byte(&SubstrateAssets::FRAMES[idx].width);
}

uint8_t TerrariumScene::substrateFrameHeight(uint8_t level) const {
    uint8_t idx = substrateFrameIndex(level);
    return pgm_read_byte(&SubstrateAssets::FRAMES[idx].height);
}

int TerrariumScene::substrateSlotCenterX(uint8_t slot) const {
    return SUBSTRATE_LEFT_X + slot * SUBSTRATE_SLOT_W;
}

bool TerrariumScene::substrateBounds(uint8_t slot, int& left, int& right) const {
    if (slot >= SUBSTRATE_SLOT_COUNT) return false;
    uint8_t level = substrateLevels[slot];
    if (level == 0) return false;
    uint8_t w = substrateFrameWidth(level);
    int cx = substrateSlotCenterX(slot);
    left = cx - w / 2;
    right = left + w;
    return true;
}

bool TerrariumScene::substrateSlotReserved(uint8_t slot) const {
    if (slot >= SUBSTRATE_SLOT_COUNT) return true;
    if (substrateLevels[slot] > 0) return true;
    if (substrateSlotReservedByDrop(slot)) return true;
    return false;
}

bool TerrariumScene::substrateSlotReservedByDrop(uint8_t slot) const {
    if (slot >= SUBSTRATE_SLOT_COUNT) return true;
    for (uint8_t i = 0; i < SUBSTRATE_DROP_SLOTS; ++i) {
        if (substrateDrops[i].active && substrateDrops[i].slot == slot) return true;
    }
    return false;
}

bool TerrariumScene::findLarvaSubstrateDropSlot(uint8_t& outSlot) const {
    uint8_t freeSlots[SUBSTRATE_SLOT_COUNT];
    uint8_t freeCount = 0;
    for (uint8_t slot = 0; slot < SUBSTRATE_SLOT_COUNT; ++slot) {
        if (!substrateSlotReserved(slot)) {
            freeSlots[freeCount++] = slot;
        }
    }
    if (freeCount == 0) return false;

    for (uint8_t i = 0; i < freeCount; ++i) {
        uint8_t slot = freeSlots[i];
        bool leftLevel2 = slot >= 1 && substrateLevels[slot - 1] == 2;
        bool rightLevel2 = slot + 1 < SUBSTRATE_SLOT_COUNT && substrateLevels[slot + 1] == 2;
        if (leftLevel2 || rightLevel2) {
            outSlot = slot;
            return true;
        }
    }

    for (uint8_t i = 0; i < freeCount; ++i) {
        uint8_t slot = freeSlots[i];
        bool leftPair = slot >= 2 &&
                        substrateLevels[slot - 1] == 1 &&
                        substrateLevels[slot - 2] == 1;
        bool splitPair = slot >= 1 && slot + 1 < SUBSTRATE_SLOT_COUNT &&
                         substrateLevels[slot - 1] == 1 &&
                         substrateLevels[slot + 1] == 1;
        bool rightPair = slot + 2 < SUBSTRATE_SLOT_COUNT &&
                         substrateLevels[slot + 1] == 1 &&
                         substrateLevels[slot + 2] == 1;
        if (leftPair || splitPair || rightPair) {
            outSlot = slot;
            return true;
        }
    }

    uint8_t bestSlot = freeSlots[(uint8_t)random(0, freeCount)];
    uint8_t bestScore = 0;
    for (uint8_t i = 0; i < freeCount; ++i) {
        uint8_t slot = freeSlots[i];
        uint8_t score = 0;
        if (slot >= 1 && substrateLevels[slot - 1] == 2) score += 2;
        if (slot + 1 < SUBSTRATE_SLOT_COUNT && substrateLevels[slot + 1] == 2) score += 2;
        if (slot >= 1 && substrateLevels[slot - 1] == 1) ++score;
        if (slot + 1 < SUBSTRATE_SLOT_COUNT && substrateLevels[slot + 1] == 1) ++score;
        if (score > bestScore || (score == bestScore && random(2) == 0)) {
            bestScore = score;
            bestSlot = slot;
        }
    }
    outSlot = bestSlot;
    return true;
}

bool TerrariumScene::hasLarvaSubstrate() const {
    for (uint8_t i = 0; i < SUBSTRATE_SLOT_COUNT; ++i) {
        if (substrateLevels[i] > 0) return true;
    }
    for (uint8_t i = 0; i < SUBSTRATE_DROP_SLOTS; ++i) {
        if (substrateDrops[i].active) return true;
    }
    return false;
}

uint8_t TerrariumScene::landedSubstrateSlots() const {
    uint8_t count = 0;
    for (uint8_t i = 0; i < SUBSTRATE_SLOT_COUNT; ++i) {
        if (substrateLevels[i] > 0) ++count;
    }
    return count;
}

uint16_t TerrariumScene::landedSubstrateBites() const {
    uint16_t total = 0;
    for (uint8_t i = 0; i < SUBSTRATE_SLOT_COUNT; ++i) {
        total += substrateBites[i];
    }
    return total;
}

uint8_t TerrariumScene::activeSubstrateDrops() const {
    uint8_t count = 0;
    for (uint8_t i = 0; i < SUBSTRATE_DROP_SLOTS; ++i) {
        if (substrateDrops[i].active) ++count;
    }
    return count;
}

void TerrariumScene::mergeLarvaSubstrate() {
    bool changed = true;
    while (changed) {
        changed = false;
        for (uint8_t leftSlot = 0; leftSlot < SUBSTRATE_SLOT_COUNT && !changed; ++leftSlot) {
            if (substrateLevels[leftSlot] != 2) continue;
            int leftA = 0;
            int rightA = 0;
            if (!substrateBounds(leftSlot, leftA, rightA)) continue;
            for (uint8_t rightSlot = leftSlot + 1; rightSlot < SUBSTRATE_SLOT_COUNT; ++rightSlot) {
                if (substrateLevels[rightSlot] != 2) continue;
                int leftB = 0;
                int rightB = 0;
                if (!substrateBounds(rightSlot, leftB, rightB)) continue;
                if (leftB - rightA <= SUBSTRATE_MERGE_VISUAL_GAP_PX) {
                    uint8_t mergeSlot = (leftSlot + rightSlot + 1) / 2;
                    if (mergeSlot != leftSlot && mergeSlot != rightSlot &&
                        (substrateLevels[mergeSlot] != 0 || substrateSlotReservedByDrop(mergeSlot))) {
                        mergeSlot = rightSlot;
                    }
                    uint8_t mergedBites = substrateBites[leftSlot] + substrateBites[rightSlot];
                    if (mergedBites == 0) {
                        mergedBites = substrateBiteCapacity(2) * 2;
                    }
                    clearSubstrateSlot(leftSlot);
                    clearSubstrateSlot(rightSlot);
                    setSubstrateBites(mergeSlot, mergedBites);
                    changed = true;
                    break;
                }
            }
        }
        if (changed) continue;

        for (uint8_t slot = 0; slot + 2 < SUBSTRATE_SLOT_COUNT; ++slot) {
            if (substrateLevels[slot] == 1 &&
                substrateLevels[slot + 1] == 1 &&
                substrateLevels[slot + 2] == 1) {
                uint8_t mergedBites = substrateBites[slot] +
                                      substrateBites[slot + 1] +
                                      substrateBites[slot + 2];
                if (mergedBites == 0) {
                    mergedBites = substrateBiteCapacity(1) * 3;
                }
                clearSubstrateSlot(slot);
                clearSubstrateSlot(slot + 2);
                setSubstrateBites(slot + 1, mergedBites);
                changed = true;
            }
        }
    }
}

bool TerrariumScene::spawnLarvaSubstrate() {
    if (substrateFadeActive) return false;
    uint8_t dropSlot = SUBSTRATE_DROP_SLOTS;
    for (uint8_t i = 0; i < SUBSTRATE_DROP_SLOTS; ++i) {
        if (!substrateDrops[i].active) {
            dropSlot = i;
            break;
        }
    }
    if (dropSlot >= SUBSTRATE_DROP_SLOTS) return false;

    uint8_t slot = 0;
    if (!findLarvaSubstrateDropSlot(slot)) return false;

    SubstrateDrop& drop = substrateDrops[dropSlot];
    drop.active = true;
    drop.slot = slot;
    drop.level = 1;
    drop.y = (float)(-substrateFrameHeight(drop.level) - random(0, 18));
    drop.vy = SUBSTRATE_DROP_START_VY + (float)random(0, 50);
    substrateLastUpdateMs = Hal::ins().millis();
    return true;
}

void TerrariumScene::updateLarvaSubstrate(uint32_t nowMs) {
    uint32_t dtMs = substrateLastUpdateMs == 0 ? 0 : nowMs - substrateLastUpdateMs;
    if (dtMs > 120) dtMs = 120;
    substrateLastUpdateMs = nowMs;
    float dt = (float)dtMs / 1000.0f;

    for (uint8_t i = 0; i < SUBSTRATE_DROP_SLOTS; ++i) {
        SubstrateDrop& drop = substrateDrops[i];
        if (!drop.active) continue;

        drop.vy += SUBSTRATE_DROP_GRAVITY * dt;
        drop.y += drop.vy * dt;

        float landingY = (float)(GROUND_Y - substrateFrameHeight(drop.level));
        if (drop.y >= landingY) {
            uint8_t slot = drop.slot;
            if (slot >= SUBSTRATE_SLOT_COUNT || substrateLevels[slot] > 0) {
                if (!findLarvaSubstrateDropSlot(slot)) {
                    drop.active = false;
                    continue;
                }
            }
            setSubstrateBites(slot, substrateBiteCapacity(drop.level));
            mergeLarvaSubstrate();
            drop.active = false;
        }
    }
}

void TerrariumScene::drawLarvaSubstrate(uint8_t opacity) {
    Bug& bug = GameEngine::ins().getBug();
    if (bug.getStage() != Stage::EGG && bug.getStage() != Stage::LARVA && !substrateFadeActive) return;

    for (uint8_t slot = 0; slot < SUBSTRATE_SLOT_COUNT; ++slot) {
        uint8_t level = substrateLevels[slot];
        if (level == 0) continue;
        uint8_t frame = substrateFrameIndex(level);
        uint16_t offset = pgm_read_word(&SubstrateAssets::FRAMES[frame].offset);
        uint16_t length = pgm_read_word(&SubstrateAssets::FRAMES[frame].length);
        uint8_t w = substrateFrameWidth(level);
        uint8_t h = substrateFrameHeight(level);
        int drawX = substrateSlotCenterX(slot) - w / 2;
        int drawY = GROUND_Y - h;
        if (opacity < 255) {
            drawRgb565RleFaded(drawX, drawY, w, h, SubstrateAssets::RLE, offset, length, opacity);
        } else if (!drawCachedRleFrame(drawX, drawY, w, h, SubstrateAssets::RLE, offset, length, 1.0f, false)) {
            PixelRenderer::drawRgb565Rle(drawX, drawY, w, h, SubstrateAssets::RLE, offset, length);
        }
    }

    if (opacity < 255) return;
    for (uint8_t i = 0; i < SUBSTRATE_DROP_SLOTS; ++i) {
        const SubstrateDrop& drop = substrateDrops[i];
        if (!drop.active) continue;
        uint8_t frame = substrateFrameIndex(drop.level);
        uint16_t offset = pgm_read_word(&SubstrateAssets::FRAMES[frame].offset);
        uint16_t length = pgm_read_word(&SubstrateAssets::FRAMES[frame].length);
        uint8_t w = substrateFrameWidth(drop.level);
        uint8_t h = substrateFrameHeight(drop.level);
        int drawX = substrateSlotCenterX(drop.slot) - w / 2;
        int drawY = (int)roundf(drop.y);
        if (!drawCachedRleFrame(drawX, drawY, w, h, SubstrateAssets::RLE, offset, length, 1.0f, false)) {
            PixelRenderer::drawRgb565Rle(drawX, drawY, w, h, SubstrateAssets::RLE, offset, length);
        }
    }
}

bool TerrariumScene::larvaCollidesWithSubstrateAt(int x) const {
    int left = x - LARVA_COLLISION_HALF_W_PX;
    int right = x + LARVA_COLLISION_HALF_W_PX;
    for (uint8_t slot = 0; slot < SUBSTRATE_SLOT_COUNT; ++slot) {
        int subLeft = 0;
        int subRight = 0;
        if (!substrateBounds(slot, subLeft, subRight)) continue;
        if (right >= subLeft && left <= subRight) return true;
    }
    return false;
}

int TerrariumScene::larvaHeadX() const {
    return bugX + (larvaFaceRight ? LARVA_HEAD_OFFSET_PX : -LARVA_HEAD_OFFSET_PX);
}

int TerrariumScene::clampLarvaStepForSubstrate(int fromX, int toX) const {
    if (larvaCollidesWithSubstrateAt(fromX)) return toX;
    if (!larvaCollidesWithSubstrateAt(toX)) return toX;
    int step = toX > fromX ? 1 : -1;
    int candidate = fromX;
    while (candidate != toX) {
        int next = candidate + step;
        if (larvaCollidesWithSubstrateAt(next)) break;
        candidate = next;
    }
    return candidate;
}

bool TerrariumScene::findLarvaSubstrateFoodTarget(int& outX, bool* outFaceRight) const {
    bool found = false;
    int bestDist = 32767;
    int bestTarget = bugX;
    bool bestFaceRight = larvaFaceRight;

    int groupLefts[SUBSTRATE_SLOT_COUNT];
    int groupRights[SUBSTRATE_SLOT_COUNT];
    uint8_t groupCount = 0;
    for (uint8_t slot = 0; slot < SUBSTRATE_SLOT_COUNT; ++slot) {
        int leftEdge = 0;
        int rightEdge = 0;
        if (!substrateBounds(slot, leftEdge, rightEdge)) continue;
        if (groupCount > 0 &&
            leftEdge <= groupRights[groupCount - 1] + SUBSTRATE_MERGE_VISUAL_GAP_PX) {
            if (rightEdge > groupRights[groupCount - 1]) {
                groupRights[groupCount - 1] = rightEdge;
            }
        } else if (groupCount < SUBSTRATE_SLOT_COUNT) {
            groupLefts[groupCount] = leftEdge;
            groupRights[groupCount] = rightEdge;
            ++groupCount;
        }
    }

    for (uint8_t group = 0; group < groupCount; ++group) {
        int leftEdge = groupLefts[group];
        int rightEdge = groupRights[group];
        int leftTarget = leftEdge - LARVA_HEAD_OFFSET_PX - LARVA_SUBSTRATE_FEED_GAP_PX;
        int leftBodySafe = leftEdge - LARVA_COLLISION_HALF_W_PX - 1;
        if (leftTarget > leftBodySafe) leftTarget = leftBodySafe;
        int rightTarget = rightEdge + LARVA_HEAD_OFFSET_PX + LARVA_SUBSTRATE_FEED_GAP_PX;
        int rightBodySafe = rightEdge + LARVA_COLLISION_HALF_W_PX + 1;
        if (rightTarget < rightBodySafe) rightTarget = rightBodySafe;
        if (leftTarget < MIN_X) leftTarget = MIN_X;
        if (rightTarget > MAX_X) rightTarget = MAX_X;

        int leftDist = abs(leftTarget - bugX);
        int rightDist = abs(rightTarget - bugX);
        bool leftUsable = !larvaCollidesWithSubstrateAt(leftTarget) &&
                          abs((leftTarget + LARVA_HEAD_OFFSET_PX) - leftEdge) <= LARVA_HEAD_FEED_DISTANCE_PX;
        bool rightUsable = !larvaCollidesWithSubstrateAt(rightTarget) &&
                           abs((rightTarget - LARVA_HEAD_OFFSET_PX) - rightEdge) <= LARVA_HEAD_FEED_DISTANCE_PX;
        int target = leftTarget;
        int dist = leftDist;
        bool faceRight = true;
        if (!leftUsable && rightUsable) {
            target = rightTarget;
            dist = rightDist;
            faceRight = false;
        } else if (leftUsable && !rightUsable) {
            target = leftTarget;
            dist = leftDist;
            faceRight = true;
        } else if (!leftUsable && !rightUsable) {
            continue;
        } else if (rightDist < leftDist) {
            target = rightTarget;
            dist = rightDist;
            faceRight = false;
        }
        if (dist < bestDist) {
            bestDist = dist;
            bestTarget = target;
            bestFaceRight = faceRight;
            found = true;
        }
    }

    if (!found) {
        for (uint8_t group = 0; group < groupCount; ++group) {
            int leftEdge = groupLefts[group];
            int rightEdge = groupRights[group];
            int leftTarget = leftEdge - LARVA_HEAD_OFFSET_PX - LARVA_SUBSTRATE_FEED_GAP_PX;
            if (leftTarget < MIN_X) leftTarget = MIN_X;
            if (leftTarget > MAX_X) leftTarget = MAX_X;
            int leftHeadDist = abs((leftTarget + LARVA_HEAD_OFFSET_PX) - leftEdge);
            if (leftHeadDist <= LARVA_HEAD_FEED_FALLBACK_DISTANCE_PX) {
                int dist = abs(leftTarget - bugX);
                if (dist < bestDist) {
                    bestDist = dist;
                    bestTarget = leftTarget;
                    bestFaceRight = true;
                    found = true;
                }
            }

            int rightTarget = rightEdge + LARVA_HEAD_OFFSET_PX + LARVA_SUBSTRATE_FEED_GAP_PX;
            if (rightTarget < MIN_X) rightTarget = MIN_X;
            if (rightTarget > MAX_X) rightTarget = MAX_X;
            int rightHeadDist = abs((rightTarget - LARVA_HEAD_OFFSET_PX) - rightEdge);
            if (rightHeadDist <= LARVA_HEAD_FEED_FALLBACK_DISTANCE_PX) {
                int dist = abs(rightTarget - bugX);
                if (dist < bestDist) {
                    bestDist = dist;
                    bestTarget = rightTarget;
                    bestFaceRight = false;
                    found = true;
                }
            }
        }
    }

    if (!found) {
        for (uint8_t slot = 0; slot < SUBSTRATE_SLOT_COUNT; ++slot) {
            int leftEdge = 0;
            int rightEdge = 0;
            if (!substrateBounds(slot, leftEdge, rightEdge)) continue;
            int center = (leftEdge + rightEdge) / 2;
            int target = center;
            if (target < MIN_X) target = MIN_X;
            if (target > MAX_X) target = MAX_X;
            int dist = abs(target - bugX);
            if (dist < bestDist) {
                bestDist = dist;
                bestTarget = target;
                bestFaceRight = center >= bugX;
                found = true;
            }
        }
    }

    outX = bestTarget;
    if (outFaceRight) *outFaceRight = bestFaceRight;
    return found;
}

int TerrariumScene::larvaSubstrateFoodSlotNearHead() const {
    int target = bugX;
    bool targetFaceRight = larvaFaceRight;
    int headX = larvaHeadX();
    int bestSlot = -1;
    int bestDist = 32767;
    for (uint8_t slot = 0; slot < SUBSTRATE_SLOT_COUNT; ++slot) {
        int leftEdge = 0;
        int rightEdge = 0;
        if (!substrateBounds(slot, leftEdge, rightEdge)) continue;
        if (headX >= leftEdge && headX <= rightEdge) {
            return slot;
        }
    }

    if (!findLarvaSubstrateFoodTarget(target, &targetFaceRight)) return -1;
    if (abs(target - bugX) > 2) return -1;
    if (larvaFaceRight != targetFaceRight) return -1;

    for (uint8_t slot = 0; slot < SUBSTRATE_SLOT_COUNT; ++slot) {
        int leftEdge = 0;
        int rightEdge = 0;
        if (!substrateBounds(slot, leftEdge, rightEdge)) continue;
        int edge = larvaFaceRight ? leftEdge : rightEdge;
        int dist = abs(headX - edge);
        if (dist <= LARVA_HEAD_FEED_FALLBACK_DISTANCE_PX && dist < bestDist) {
            bestDist = dist;
            bestSlot = slot;
        }
    }
    if (bestSlot < 0) {
        int bodyLeft = bugX - LARVA_COLLISION_HALF_W_PX;
        int bodyRight = bugX + LARVA_COLLISION_HALF_W_PX;
        for (uint8_t slot = 0; slot < SUBSTRATE_SLOT_COUNT; ++slot) {
            int leftEdge = 0;
            int rightEdge = 0;
            if (!substrateBounds(slot, leftEdge, rightEdge)) continue;
            if (bodyRight < leftEdge || bodyLeft > rightEdge) continue;
            int center = (leftEdge + rightEdge) / 2;
            int dist = abs(center - bugX);
            if (dist < bestDist) {
                bestDist = dist;
                bestSlot = slot;
            }
        }
    }
    return bestSlot;
}

bool TerrariumScene::larvaNearSubstrateFood() const {
    return larvaSubstrateFoodSlotNearHead() >= 0;
}

bool TerrariumScene::consumeLarvaSubstrateBite(Bug& bug, uint32_t nowMs) {
    if (bug.getHunger() >= 100) return false;
    if (!larvaNearSubstrateFood()) return false;
    uint32_t biteIntervalMs = larvaSubstrateBiteIntervalMs(bug.getHunger());
    if (larvaLastSubstrateBiteMs != 0 &&
        nowMs - larvaLastSubstrateBiteMs < biteIntervalMs) {
        return false;
    }

    int nearestSlot = larvaSubstrateFoodSlotNearHead();
    if (nearestSlot < 0) return false;

    uint64_t gameNow = GameEngine::ins().getGameNow();
    if (!bug.eatSubstrateBite(gameNow, Bug::LARVA_SUBSTRATE_HUNGER_GAIN)) return false;
    observedLarvaEatGameMs = bug.getLastEatTime();
    larvaLastSubstrateBiteMs = nowMs;
    if (substrateBites[nearestSlot] > 0) {
        setSubstrateBites(nearestSlot, substrateBites[nearestSlot] - 1);
    }
    if (larvaState == LarvaState::EAT) {
        uint32_t elapsed = nowMs - larvaStateStartMs;
        uint32_t minDuration = elapsed + random(LARVA_EAT_EMPTY_EXIT_MS, LARVA_EAT_EMPTY_EXIT_MS + 1800);
        if (larvaStateDurationMs < minDuration) larvaStateDurationMs = minDuration;
    } else {
        enterLarvaState(LarvaState::EAT, nowMs);
    }
    larvaTargetX = bugX;
    return true;
}
