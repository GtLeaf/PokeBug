void ExploreScene::onEnter() {
    Bug& bug = GameEngine::ins().getBug();

    // 检查是否刚从 NPC 对战返回
    NpcBattleResult& res = GameEngine::ins().lastNpcBattleResult();
    if (res.valid && res.fromExplore) {
        restoreSession();
        applyNpcBattleResult(res);
        GameEngine::ins().clearLastNpcBattleResult();
        return;
    }

    startNewSession();

    // 首次进入：扣除饱腹值 20
    if (bug.getHunger() >= 30) {
        bug.modHunger(-20);
    }

    startRoundWait(Hal::ins().millis());
    eventType = EventType::NOTHING;
    resultLine1[0] = '\0';
    resultLine2[0] = '\0';
    Serial.printf("[Explore] Enter location=%s tod=%s\n",
                  GameEngine::ins().getExploreLocationName(),
                  GameEngine::ins().getTimeOfDayShortName());
}

void ExploreScene::onExit() {
    Serial.println("[Explore] Exit");
}

void ExploreScene::startNewSession() {
    currentRound = 1;
    totalSapGain = 0;
    finalRecorded = false;
    releaseEventSeen = false;
    saveSession();
}

void ExploreScene::restoreSession() {
    if (!sSessionActive) {
        startNewSession();
        return;
    }
    currentRound = sCurrentRound;
    totalSapGain = sTotalSapGain;
    finalRecorded = sFinalRecorded;
    releaseEventSeen = sReleaseEventSeen;
}

void ExploreScene::saveSession() {
    sSessionActive = true;
    sCurrentRound = currentRound;
    sTotalSapGain = totalSapGain;
    sFinalRecorded = finalRecorded;
    sReleaseEventSeen = releaseEventSeen;
}

void ExploreScene::clearSession() {
    sSessionActive = false;
    sCurrentRound = 1;
    sTotalSapGain = 0;
    sFinalRecorded = false;
    sReleaseEventSeen = false;
}

SceneID ExploreScene::update() {
    uint32_t now = Hal::ins().millis();

    if (state == State::EXPLORING) {
        if (now - roundStartedAtMs >= EXPLORE_EVENT_DELAY_MS) {
            triggerEvent(now);
        }
    }

    if (state == State::EVENT_REVEAL && now - revealStartedAtMs >= EXPLORE_REVEAL_SHAKE_MS) {
        state = pendingRevealState;
        saveSession();
    }

    if (state == State::RETURNING) {
        clearSession();
        return SCENE_TERRARIUM;
    }

    return nextScene;
}

void ExploreScene::startRoundWait(uint32_t now) {
    roundStartedAtMs = now;
    revealStartedAtMs = 0;
    pendingRevealState = State::EVENT_POPUP;
    state = State::EXPLORING;
}

void ExploreScene::beginEventReveal(uint32_t now, State nextState) {
    revealStartedAtMs = now;
    pendingRevealState = nextState;
    state = State::EVENT_REVEAL;
    saveSession();
}

int ExploreScene::revealShakeOffset(uint32_t now) const {
    if (revealStartedAtMs == 0 || now - revealStartedAtMs >= EXPLORE_REVEAL_SHAKE_MS) {
        return 0;
    }
    return ((now - revealStartedAtMs) / 45) % 2 == 0 ? -EXPLORE_REVEAL_SHAKE_PX : EXPLORE_REVEAL_SHAKE_PX;
}

void ExploreScene::triggerEvent(uint32_t now) {
    Bug& bug = GameEngine::ins().getBug();
    uint8_t location = GameEngine::ins().getExploreLocation();
    uint8_t tod = GameEngine::ins().getTimeOfDay();
    if (location >= GameEngine::EXPLORE_LOCATION_COUNT) location = GameEngine::EXPLORE_PARK;
    if (tod > GameEngine::TIME_EVENING) tod = GameEngine::TIME_MORNING;

    const ExploreEventWeights& weights = EVENT_TABLE[location][tod];
    int roll = random(100);
    int acc = weights.sap;
    if (roll < acc) {
        eventType = EventType::SAP;
        eventValue = random(1, 4); // 1-3
    } else if (roll < (acc += weights.food)) {
        eventType = EventType::FOOD_SOURCE;
        eventValue = random(1, 3); // 1-2
    } else if (roll < (acc += weights.wood)) {
        eventType = EventType::WOOD;
    } else if (roll < (acc += weights.npc)) {
        eventType = EventType::NPC;
    } else if (roll < (acc += weights.rare)) {
        eventType = EventType::RARE;
        rareSubType = random(6);
    } else if (roll < (acc += weights.release)) {
        eventType = EventType::RELEASE;
    } else {
        eventType = EventType::NOTHING;
    }

    if (eventType == EventType::RELEASE && releaseEventSeen) {
        eventType = EventType::NOTHING;
    }

    if (eventType == EventType::RELEASE) {
        releaseEventSeen = true;
        snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_RELEASE_EVENT);
        snprintf(resultLine2, sizeof(resultLine2), "%s", UiStrings::EXPLORE_RELEASE_EGG);
        beginEventReveal(now, State::RELEASE_PROMPT);
        return;
    }

    if (eventType == EventType::NPC) {
        if (bug.getStage() != Stage::ADULT) {
            if (!finalRecorded) {
                GameEngine::ins().recordExploreFinished();
                finalRecorded = true;
            }
            snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_TOO_YOUNG_FIGHT);
            snprintf(resultLine2, sizeof(resultLine2), "%s", UiStrings::EXPLORE_END);
            beginEventReveal(now, State::FINAL_SUMMARY);
            return;
        }

        npc = NpcGenerator::generateForExplore(bug, NPC_TIER_TABLE[location][tod]);
        // 读取 PROGMEM 名字/台词
        strncpy_P(npcName, (const char*)pgm_read_ptr(&NpcData::ENTRIES[npc.index].name), sizeof(npcName) - 1);
        npcName[sizeof(npcName) - 1] = '\0';
        strncpy_P(npcBugName, (const char*)pgm_read_ptr(&NpcData::ENTRIES[npc.index].bugName), sizeof(npcBugName) - 1);
        npcBugName[sizeof(npcBugName) - 1] = '\0';
        strncpy_P(npcMeetLine, (const char*)pgm_read_ptr(&NpcData::ENTRIES[npc.index].meet), sizeof(npcMeetLine) - 1);
        npcMeetLine[sizeof(npcMeetLine) - 1] = '\0';
        beginEventReveal(now, State::NPC_PROMPT);
        return;
    }

    applyEventReward();
    beginEventReveal(now, State::EVENT_POPUP);
}

void ExploreScene::applyEventReward(bool flee) {
    Bug& bug = GameEngine::ins().getBug();
    resultLine1[0] = '\0';
    resultLine2[0] = '\0';
    uint8_t location = GameEngine::ins().getExploreLocation();
    if (location >= GameEngine::EXPLORE_LOCATION_COUNT) location = GameEngine::EXPLORE_PARK;
    auto locationFood = [&]() -> FoodType {
        switch (location) {
            case GameEngine::EXPLORE_PARK: return FoodType::CUBE;
            case GameEngine::EXPLORE_BACK_HILL: return FoodType::SLICE;
            case GameEngine::EXPLORE_RIVERSIDE: return FoodType::CITRUS;
            case GameEngine::EXPLORE_OLD_WOODS: return FoodType::BERRY;
            default: return FoodType::DROP;
        }
    };
    auto addLocationFood = [&](uint8_t amount) {
        FoodType type = locationFood();
        bool newlyUnlocked = GameEngine::ins().unlockFood(type);
        bug.addFood(type, amount);
        totalSapGain += amount;
        if (newlyUnlocked) {
            snprintf(resultLine1, sizeof(resultLine1), "%s unlocked", FoodTypeInfo::name(type));
        } else {
            snprintf(resultLine1, sizeof(resultLine1), "%s +%d", FoodTypeInfo::name(type), amount);
        }
    };

    switch (eventType) {
        case EventType::SAP:
            bug.addFood(FoodType::DROP, eventValue);
            totalSapGain += eventValue;
            snprintf(resultLine1, sizeof(resultLine1), UiStrings::EXPLORE_SAP_PLUS, eventValue);
            break;
        case EventType::FOOD_SOURCE:
            addLocationFood(eventValue);
            break;
        case EventType::WOOD:
            bug.addFood(FoodType::DROP, 2);
            totalSapGain += 2;
            snprintf(resultLine1, sizeof(resultLine1), UiStrings::EXPLORE_SAP_PLUS, 2);
            break;
        case EventType::RARE: {
            uint8_t location = GameEngine::ins().getExploreLocation();
            uint8_t tod = GameEngine::ins().getTimeOfDay();
            if (location >= GameEngine::EXPLORE_LOCATION_COUNT) location = GameEngine::EXPLORE_PARK;
            if (tod > GameEngine::TIME_EVENING) tod = GameEngine::TIME_MORNING;

            auto addSap = [&](uint8_t amount) {
                bug.addFood(FoodType::DROP, amount);
                totalSapGain += amount;
            };
            auto addStat = [&](float siz, float str, float end, float spd, float spi, const char* text) {
                bug.addTrainingBonus(siz, str, end, spd, spi);
                snprintf(resultLine2, sizeof(resultLine2), "%s", text);
            };

            switch (location) {
                case GameEngine::EXPLORE_PARK:
                    switch (rareSubType) {
                        case 0: {
                            uint8_t sap = (uint8_t)random(2, 5);
                            if (tod == GameEngine::TIME_AFTERNOON) sap++;
                            addLocationFood(sap);
                            if (resultLine2[0] == '\0') {
                                snprintf(resultLine2, sizeof(resultLine2), UiStrings::EXPLORE_PICNIC_CRUMBS, sap);
                            }
                            break;
                        }
                        case 1:
                            addSap(1);
                            snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_SPRINKLER);
                            break;
                        case 2:
                            addSap(2);
                            snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_MORNING_DEW);
                            break;
                        case 3:
                            addSap(1);
                            snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_EARTHWORM);
                            break;
                        case 4:
                            addSap(3);
                            addStat(0.0f, 0.0f, 0.0f, 0.0f, 0.1f, UiStrings::EXPLORE_SPI_PLUS);
                            snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_RAINBOW);
                            break;
                        case 5:
                        default:
                            addSap(1);
                            snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_DANDELION);
                            break;
                    }
                    break;

                case GameEngine::EXPLORE_BACK_HILL:
                    switch (rareSubType) {
                        case 0:
                            addSap(1);
                            snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_SUN_BARK);
                            break;
                        case 1:
                            addLocationFood(2);
                            snprintf(resultLine2, sizeof(resultLine2), "%s", UiStrings::EXPLORE_ANT_TRAIL);
                            break;
                        case 2:
                            addSap(1);
                            snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_PINECONE);
                            break;
                        case 3:
                            addSap(2);
                            snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_CRACKED_STUMP);
                            break;
                        case 4:
                            addSap(2);
                            addStat(0.0f, 0.0f, 0.1f, 0.0f, 0.0f, UiStrings::EXPLORE_END_PLUS_0_1);
                            snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_LIZARD);
                            break;
                        case 5:
                        default:
                            snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_CAT_TRACKS);
                            break;
                    }
                    break;

                case GameEngine::EXPLORE_RIVERSIDE:
                    switch (rareSubType) {
                        case 0:
                            addSap(2);
                            snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_MOSS_CARPET);
                            break;
                        case 1:
                            addLocationFood(3);
                            addStat(0.0f, 0.0f, 0.0f, 0.0f, 0.2f, UiStrings::EXPLORE_SPI_PLUS);
                            if (resultLine2[0] == '\0') snprintf(resultLine2, sizeof(resultLine2), "%s", UiStrings::EXPLORE_FIREFLIES);
                            break;
                        case 2:
                            addSap(1);
                            snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_BULLFROG);
                            break;
                        case 3:
                            addSap(3);
                            addStat(0.0f, 0.0f, 0.2f, 0.0f, 0.0f, UiStrings::EXPLORE_END_PLUS_0_2);
                            snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_FAIRY_RING);
                            break;
                        case 4:
                            addSap(2);
                            snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_DRIFTWOOD);
                            break;
                        case 5:
                        default:
                            addSap(2);
                            snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_WATER_STRIDER);
                            break;
                    }
                    break;

                case GameEngine::EXPLORE_OLD_WOODS:
                default:
                    switch (rareSubType) {
                        case 0:
                            addSap(4);
                            addStat(0.0f, 0.0f, 0.3f, 0.0f, 0.0f, UiStrings::EXPLORE_END_PLUS_0_3);
                            snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_ANCIENT_RESIN);
                            break;
                        case 1:
                            addLocationFood(2);
                            snprintf(resultLine2, sizeof(resultLine2), "%s", UiStrings::EXPLORE_DEER_MUSHROOM);
                            break;
                        case 2:
                            addSap(3);
                            snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_DEADWOOD_BEETLES);
                            break;
                        case 3:
                            addSap(5);
                            snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_GLOW_MOSS);
                            break;
                        case 4:
                            addSap(2);
                            addStat(0.0f, 0.2f, 0.0f, 0.0f, 0.0f, UiStrings::EXPLORE_STR_PLUS_0_2);
                            snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_HEART_OF_ROT);
                            break;
                        case 5:
                        default:
                            addSap(6);
                            addStat(0.0f, 0.0f, 0.0f, 0.0f, 0.3f, UiStrings::EXPLORE_SPI_PLUS);
                            snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_OLD_PHANTOM);
                            break;
                    }
                    break;
            }
            break;
        }
        case EventType::RELEASE:
            snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_RELEASE_EVENT);
            snprintf(resultLine2, sizeof(resultLine2), "%s", UiStrings::EXPLORE_RELEASE_EGG);
            break;
        case EventType::NOTHING:
            snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_NOTHING_HAPPENED);
            break;
        default:
            break;
    }
}

void ExploreScene::advanceRoundOrFinish() {
    if (currentRound >= MAX_EXPLORE_ROUNDS) {
        enterFinalSummary(UiStrings::EXPLORE_COMPLETE);
        return;
    }
    currentRound++;
    resultLine1[0] = '\0';
    resultLine2[0] = '\0';
    startRoundWait(Hal::ins().millis());
    saveSession();
}

void ExploreScene::enterFinalSummary(const char* line1, const char* line2) {
    if (!finalRecorded) {
        GameEngine::ins().recordExploreFinished();
        finalRecorded = true;
    }
    snprintf(resultLine1, sizeof(resultLine1), "%s", line1 ? line1 : UiStrings::EXPLORE_COMPLETE);
    if (line2) {
        snprintf(resultLine2, sizeof(resultLine2), "%s", line2);
    } else {
        snprintf(resultLine2, sizeof(resultLine2), UiStrings::EXPLORE_SAP_PLUS, totalSapGain);
    }
    state = State::FINAL_SUMMARY;
    saveSession();
}

void ExploreScene::startNpcBattle() {
    saveSession();
    GameEngine::ins().setPendingNpcBattle(npc, SCENE_EXPLORE, npc.tier == NpcData::Tier::LEGEND, true, false);
    nextScene = SCENE_BATTLE;
}

void ExploreScene::applyNpcBattleResult(const NpcBattleResult& res) {
    Bug& bug = GameEngine::ins().getBug();
    resultLine1[0] = '\0';
    resultLine2[0] = '\0';
    if (res.won) {
        uint8_t location = GameEngine::ins().getExploreLocation();
        if (location >= GameEngine::EXPLORE_LOCATION_COUNT) location = GameEngine::EXPLORE_PARK;
        auto locationFood = [&]() -> FoodType {
            switch (location) {
                case GameEngine::EXPLORE_PARK: return FoodType::CUBE;
                case GameEngine::EXPLORE_BACK_HILL: return FoodType::JELLY;
                case GameEngine::EXPLORE_RIVERSIDE: return FoodType::CITRUS;
                case GameEngine::EXPLORE_OLD_WOODS: return FoodType::BERRY;
                default: return FoodType::DROP;
            }
        };
        int sap = 0;
        switch (res.tier) {
            case NpcData::Tier::ROOKIE:  sap = 1; break;
            case NpcData::Tier::NORMAL:  sap = random(1, 3); break;
            case NpcData::Tier::VETERAN: sap = random(2, 4); break;
            case NpcData::Tier::LEGEND:  sap = random(3, 6); break;
            default: break;
        }
        FoodType rewardFood = locationFood();
        bool foodUnlocked = GameEngine::ins().unlockFood(rewardFood);
        bug.addFood(rewardFood, sap);
        totalSapGain += sap;
        if (foodUnlocked) {
            snprintf(resultLine1, sizeof(resultLine1), "%s unlocked", FoodTypeInfo::name(rewardFood));
        } else {
            snprintf(resultLine1, sizeof(resultLine1), "Victory! %s +%d", FoodTypeInfo::name(rewardFood), sap);
        }
        if (location == GameEngine::EXPLORE_PARK &&
            GameEngine::ins().unlockMainSceneBg(GameEngine::BG_SCHOOL)) {
            snprintf(resultLine2, sizeof(resultLine2), "School unlocked");
        }
        if (location == GameEngine::EXPLORE_OLD_WOODS && res.legend &&
            GameEngine::ins().unlockFeature(GameEngine::FEATURE_CUP_UNLOCKED)) {
            snprintf(resultLine2, sizeof(resultLine2), "Cup unlocked");
        }
        if (res.spiBoosted) {
            snprintf(resultLine2, sizeof(resultLine2), "%s", UiStrings::EXPLORE_SPI_PLUS);
        }
        state = State::EVENT_POPUP;
        saveSession();
    } else {
        int sapLoss = 0;
        switch (res.tier) {
            case NpcData::Tier::ROOKIE:  sapLoss = 0; break;
            case NpcData::Tier::NORMAL:  sapLoss = 1; break;
            case NpcData::Tier::VETERAN: sapLoss = 2; break;
            case NpcData::Tier::LEGEND:  sapLoss = 99; break; // 清零
            default: break;
        }
        char lossBuf[32];
        snprintf(lossBuf, sizeof(lossBuf), "%s", UiStrings::EXPLORE_NO_PENALTY);
        if (sapLoss > 0) {
            uint8_t have = bug.getFoodCount(FoodType::DROP);
            if (sapLoss > have) sapLoss = have;
            if (sapLoss > 0) {
                bug.removeFood(FoodType::DROP, sapLoss);
                snprintf(lossBuf, sizeof(lossBuf), UiStrings::EXPLORE_LOSS_SAP_FMT, sapLoss);
            }
        }
        snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_DEFEATED);
        snprintf(resultLine2, sizeof(resultLine2), "%s", lossBuf);
        bug.setMot(0);
        enterFinalSummary(UiStrings::EXPLORE_DEFEATED, UiStrings::EXPLORE_MOT_ZERO);
    }
}

void ExploreScene::releaseForEgg() {
    Bug& bug = GameEngine::ins().getBug();
    bug.release(GameEngine::ins().getGameNow());
    GameEngine::ins().forceSave();
    enterFinalSummary(UiStrings::EXPLORE_RELEASED, UiStrings::EXPLORE_NEW_EGG);
}

bool ExploreScene::onButton(const ButtonEvent& ev) {
    if (ev.action == BtnAction::LONG_PRESS) {
        if (ev.btn == 1) {
            // 长按 B 中断探索
            enterFinalSummary(UiStrings::EXPLORE_END);
            return true;
        }
    }

    if (ev.action != BtnAction::PRESSED) return false;

    if (state == State::EXPLORING) {
        if (ev.btn == 1) {
            // 短按 B 中断
            enterFinalSummary(UiStrings::EXPLORE_END);
            return true;
        }
        return false;
    }

    if (state == State::EVENT_POPUP) {
        if (ev.btn == 0) {
            advanceRoundOrFinish();
            return true;
        }
        if (ev.btn == 1) {
            enterFinalSummary(UiStrings::EXPLORE_END);
            return true;
        }
        return false;
    }

    if (state == State::RELEASE_PROMPT) {
        if (ev.btn == 0) {
            releaseForEgg();
            return true;
        }
        if (ev.btn == 1) {
            enterFinalSummary(UiStrings::EXPLORE_LEFT, UiStrings::EXPLORE_SAFE_RETURN);
            return true;
        }
        return false;
    }

    if (state == State::FINAL_SUMMARY) {
        if (ev.btn == 0) {
            state = State::RETURNING;
            return true;
        }
        return false;
    }

    if (state == State::NPC_PROMPT) {
        if (ev.btn == 0) {
            startNpcBattle();
            return true;
        }
        if (ev.btn == 1) {
            enterFinalSummary(UiStrings::EXPLORE_LEFT, UiStrings::EXPLORE_SAFE_RETURN);
            return true;
        }
        return false;
    }

    return false;
}
