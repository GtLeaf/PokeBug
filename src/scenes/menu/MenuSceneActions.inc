bool MenuScene::onButton(const ButtonEvent& ev) {
    if (sleepTransitionActive) {
        return true;
    }

    if (giftQuantityActive) {
        return handleGiftQuantityButton(ev);
    }

    if (attrEditActive) {
        return handleAttrEditButton(ev);
    }

    // Toast 激活时任意按键关闭弹窗
    if (toastMsg && Hal::ins().millis() < toastEndMs) {
        if (ev.action == BtnAction::PRESSED) {
            toastMsg = nullptr;
            return true;
        }
    }

    // Sleep 确认对话框
    if (sleepConfirmActive) {
        if (ev.action == BtnAction::PRESSED) {
            if (ev.btn == 0) {
                // A：确认睡觉
                executeSleep();
            } else if (ev.btn == 1) {
                // B：取消
                sleepConfirmActive = false;
            }
            return true;
        }
        return false;
    }

    if (ev.action == BtnAction::LONG_PRESS) {
        if (ev.btn == 0) {
            // 长按 A：返回主界面（培养缸）
            nextScene = SCENE_TERRARIUM;
            return true;
        }
        if (ev.btn == 1) {
            // 长按 B：返回上一级
            if (mode == Mode::FOOD) {
                enterMode(Mode::MAIN);
            } else if (mode == Mode::GIFT_FOOD) {
                enterMode(Mode::GIFT);
            } else if (mode == Mode::TOY) {
                enterMode(Mode::BOX);
            } else if (mode == Mode::BOWL) {
                enterMode(Mode::BOX);
            } else if (mode == Mode::WOOD) {
                enterMode(Mode::BOX);
            } else if (mode == Mode::BOX) {
                enterMode(Mode::MAIN);
            } else if (mode == Mode::GIFT) {
                enterMode(Mode::SOCIAL);
            } else if (mode == Mode::FIGHT) {
                enterMode(Mode::SOCIAL);
            } else if (mode == Mode::VISIT) {
                enterMode(Mode::SOCIAL);
            } else if (mode == Mode::SOCIAL) {
                enterMode(Mode::MAIN);
            } else if (mode == Mode::EXPLORE) {
                enterMode(Mode::MAIN);
            } else if (mode == Mode::DEBUG) {
                enterMode(Mode::MAIN);
            } else if (mode == Mode::DEBUG_STATE || mode == Mode::DEBUG_ATTR) {
                enterMode(Mode::DEBUG);
            } else {
                nextScene = SCENE_TERRARIUM;
            }
            return true;
        }
    }

    if (ev.action == BtnAction::PRESSED) {
        if (ev.btn == 1) {
            // B：下一个（循环），从末尾跳回首项时直接跳变
            selected++;
            if (selected >= itemCount()) {
                selected = 0;
                animSelected = 0.0f;
            }
            return true;
        }
        if (ev.btn == 0) {
            // A：确认
            if (mode == Mode::FOOD) {
                // 食物子菜单：A 将当前高亮项设为全局食物（左侧菱形标记跟随），Back 返回箱子
                if (selected < FOOD_ITEM_COUNT - 1) {
                    Bug& bug = GameEngine::ins().getBug();
                    if (bug.getFoodCount((FoodType)selected) > 0) {
                        GameEngine::ins().setFoodStyle((uint8_t)selected);
                        foodConfirmTime = GameEngine::ins().getGameNow() + FOOD_CONFIRM_MS;
                        Serial.printf("[Menu] Global food set to: %s\n", FoodTypeInfo::name((FoodType)selected));
                    }
                } else {
                    enterMode(Mode::BOX);
                }
            } else if (mode == Mode::GIFT_FOOD) {
                if (isGiftFoodBackIndex(selected)) {
                    enterMode(Mode::GIFT);
                } else {
                    FoodType type = giftFoodAt(selected);
                    ItemId itemId = ItemCatalog::food(type);
                    uint8_t count = GameEngine::ins().getBug().getItemCount(itemId);
                    if (count == 0) {
                        showToast(UiStrings::GIFT_NO_FOOD);
                    } else {
                        openGiftQuantity(itemId, count);
                    }
                }
            } else {
                executeSelection();
            }
            return true;
        }
    }

    return false;
}

void MenuScene::executeSelection() {
    Bug& bug = GameEngine::ins().getBug();
    if (mode == Mode::TOY) {
        if (selected == TOY_BACK) {
            enterMode(Mode::BOX);
            return;
        }
        if (selected == TOY_NONE || selected == TOY_BALL) {
            GameEngine::ins().setToyStyle((uint8_t)selected);
            saveSettingsNow();
            Serial.printf("[Menu] Toy style: %s\n", GameEngine::ins().getToyStyleName());
        }
        return;
    }

    if (mode == Mode::FOOD) {
        // 食物子菜单的放置/返回已在 onButton 中直接处理
        if (selected == FOOD_BACK) {
            enterMode(Mode::BOX);
        }
        return;
    }

    if (mode == Mode::BOWL) {
        if (careItemsNotNeeded(bug.getStage())) {
            GameEngine::ins().setBowlStyle(0xFF);
            bug.setFoodTray(0, (FoodType)GameEngine::ins().getFoodStyle());
            saveSettingsNow();
            showToast(UiStrings::CARE_ITEM_NOT_NEEDED);
            return;
        }
        if (selected >= 0 && selected < BOWL_ITEM_COUNT - 1) {
            GameEngine::ins().setBowlStyle((uint8_t)selected);
            bug.setFoodTray(GameEngine::ins().getBowlStyle() + 1,
                            (FoodType)GameEngine::ins().getFoodStyle());
            saveSettingsNow();
            Serial.printf("[Menu] Bowl style: %s\n", GameEngine::ins().getBowlStyleName());
        } else {
            enterMode(Mode::BOX);
        }
        return;
    }

    if (mode == Mode::WOOD) {
        if (careItemsNotNeeded(bug.getStage())) {
            bug.removeWood();
            GameEngine::ins().setWoodStyle(0xFF);
            saveSettingsNow();
            showToast(UiStrings::CARE_ITEM_NOT_NEEDED);
            return;
        }
        if (selected == WOOD_NONE) {
            // 选择空选项：移除主界面的腐木
            bug.removeWood();
            GameEngine::ins().setWoodStyle(0xFF);  // 0xFF 表示未选择任何腐木风格
            saveSettingsNow();
            Serial.printf("[Menu] Wood removed\n");
        } else if (selected >= 0 && selected < WOOD_ITEM_COUNT - 1) {
            uint8_t style = (uint8_t)(selected - 1);
            if (!bug.isWoodUnlocked(style)) {
                showToast(UiStrings::WOOD_NEED_ROTTEN);
                return;
            }
            GameEngine::ins().setWoodStyle(style);
            bug.setWood(GameEngine::ins().getWoodStyle());
            saveSettingsNow();
            // 若腐木尚未放置且已解锁，自动放置以便主界面立刻显示
            if (!bug.isWoodPlaced() && !bug.placeWood()) {
                showToast(UiStrings::WOOD_NEED_ROTTEN);
            }
            Serial.printf("[Menu] Wood style: %s placed=%d\n", GameEngine::ins().getWoodStyleName(),
                          bug.isWoodPlaced());
        } else {
            enterMode(Mode::BOX);
        }
        return;
    }

    if (mode == Mode::BOX) {
        switch (selected) {
            case BOX_FOOD:
                enterMode(Mode::FOOD);
                break;
            case BOX_WOOD:
                if (careItemsNotNeeded(bug.getStage())) {
                    bug.removeWood();
                    GameEngine::ins().setWoodStyle(0xFF);
                    saveSettingsNow();
                    showToast(UiStrings::CARE_ITEM_NOT_NEEDED);
                } else {
                    enterMode(Mode::WOOD);
                }
                break;
            case BOX_BOWL:
                if (careItemsNotNeeded(bug.getStage())) {
                    GameEngine::ins().setBowlStyle(0xFF);
                    bug.setFoodTray(0, (FoodType)GameEngine::ins().getFoodStyle());
                    saveSettingsNow();
                    showToast(UiStrings::CARE_ITEM_NOT_NEEDED);
                } else {
                    enterMode(Mode::BOWL);
                }
                break;
            case BOX_BG:
                GameEngine::ins().cycleMainSceneBg();
                saveSettingsNow();
                Serial.printf("[Menu] Main scene bg: %s\n", GameEngine::ins().getMainSceneBgName());
                break;
            case BOX_TOY:
                enterMode(Mode::TOY);
                break;
            case BOX_SLEEP:
                if (bug.isDead()) {
                    showToast(UiStrings::SLEEP_BEETLE_DIED);
                } else if (!GameEngine::ins().canSleep()) {
                    showToast(UiStrings::SLEEP_TOO_EARLY);
                } else {
                    sleepConfirmActive = true;
                }
                break;
            case BOX_BACK:
                enterMode(Mode::MAIN);
                break;
        }
        return;
    }

    if (mode == Mode::SOCIAL) {
        if (selected == SOCIAL_GIFT) {
            enterMode(Mode::GIFT);
        } else if (selected == SOCIAL_FIGHT) {
            enterMode(Mode::FIGHT);
        } else if (selected == SOCIAL_VISIT) {
            enterMode(Mode::VISIT);
        } else if (selected == SOCIAL_BACK) {
            enterMode(Mode::MAIN);
        }
        return;
    }

    if (mode == Mode::GIFT) {
        if (selected == GIFT_FOOD) {
            if (giftFoodCount() == 0) {
                showToast(UiStrings::GIFT_NO_FOOD);
                return;
            }
            enterMode(Mode::GIFT_FOOD);
        } else if (selected == GIFT_RECEIVE) {
            GameEngine::ins().clearPendingGiftItem();
            GameEngine::ins().setLobbyMode(LobbyMode::LOBBY_GIFT_RECEIVE);
            nextScene = SCENE_LOBBY;
        } else if (selected == GIFT_BACK) {
            enterMode(Mode::SOCIAL);
        }
        return;
    }

    if (mode == Mode::FIGHT) {
        if (selected == FIGHT_CREATE) {
            GameEngine::ins().setLobbyMode(LobbyMode::LOBBY_CREATE);
            nextScene = SCENE_LOBBY;
        } else if (selected == FIGHT_SEARCH) {
            GameEngine::ins().setLobbyMode(LobbyMode::LOBBY_SEARCH);
            nextScene = SCENE_LOBBY;
        } else if (selected == FIGHT_BACK) {
            enterMode(Mode::SOCIAL);
        }
        return;
    }

    if (mode == Mode::VISIT) {
        if (selected == VISIT_CREATE) {
            GameEngine::ins().setLobbyMode(LobbyMode::LOBBY_VISIT_CREATE);
            nextScene = SCENE_LOBBY;
        } else if (selected == VISIT_SEARCH) {
            GameEngine::ins().setLobbyMode(LobbyMode::LOBBY_VISIT_SEARCH);
            nextScene = SCENE_LOBBY;
        } else if (selected == VISIT_BACK) {
            enterMode(Mode::SOCIAL);
        }
        return;
    }

    if (mode == Mode::EXPLORE) {
        if (selected == LOCATION_BACK) {
            enterMode(Mode::MAIN);
            return;
        }

        if (selected == LOCATION_CUP) {
            if (bug.getStage() != Stage::ADULT) {
                showToast(UiStrings::CUP_NEED_ADULT);
            } else if (bug.isDead()) {
                showToast(UiStrings::CUP_BEETLE_DIED);
            } else if (bug.getHunger() < 30) {
                showToast(UiStrings::CUP_NEED_HUNGER);
            } else if (GameEngine::ins().getCupCycleState() != GameEngine::CupCycleState::REGISTER_OPEN) {
                showToast(cupClosedMessage());
            } else {
                nextScene = SCENE_CUP;
            }
            return;
        }

        if (selected >= 0 && selected < GameEngine::EXPLORE_LOCATION_COUNT) {
            bool clockBlocked = !GameEngine::ins().isExploreTimeAllowed();
            if (clockBlocked) {
                showToast(UiStrings::EXPLORE_NIGHT_FORBIDDEN);
                return;
            }
            if (bug.getStage() != Stage::ADULT) {
                showToast(UiStrings::EXPLORE_NEED_ADULT);
            } else if (bug.isDead()) {
                showToast(UiStrings::EXPLORE_BEETLE_DIED);
            } else if (bug.getHunger() < 30) {
                showToast(UiStrings::EXPLORE_NEED_HUNGER);
            } else if (bug.getMot() < 50) {
                showToast(UiStrings::EXPLORE_MOT_LOW);
            } else if (!GameEngine::isExploreLimitBypassed() &&
                       GameEngine::ins().getExploreCountToday() >= GameEngine::EXPLORE_DAILY_LIMIT) {
                showToast(UiStrings::EXPLORE_DAILY_LIMIT);
            } else {
                GameEngine::ins().setExploreLocation((uint8_t)selected);
                Serial.printf("[Menu] Explore location: %s\n", GameEngine::ins().getExploreLocationName());
                nextScene = SCENE_EXPLORE;
            }
        }
        return;
    }

    if (mode == Mode::DEBUG) {
        if (selected == DEBUG_BACK) {
            enterMode(Mode::MAIN);
            return;
        }

        if (selected == DEBUG_BEETLE) {
            enterMode(Mode::DEBUG_STATE);
        } else if (selected == DEBUG_ATTR) {
            enterMode(Mode::DEBUG_ATTR);
        } else if (selected == DEBUG_NPC) {
            if (bug.isDead()) {
                showToast(UiStrings::EXPLORE_BEETLE_DIED);
            } else {
                NpcCombatant npc = NpcGenerator::generateForExplore(bug);
                GameEngine::ins().setPendingNpcBattle(npc, SCENE_MENU,
                                                      npc.tier == NpcData::Tier::LEGEND,
                                                      false, false);
                nextScene = SCENE_BATTLE;
                Serial.printf("[Menu] Debug VS NPC generated, tier=%d index=%d\n",
                              (int)npc.tier, npc.index);
            }
        } else if (selected == DEBUG_CLEAR_SUBSTRATE) {
            GameEngine::ins().scheduleDebugLarvaSubstrateFade(3000);
            nextScene = SCENE_TERRARIUM;
            Serial.println("[Menu] Debug substrate fade scheduled");
        }
        return;
    }

    if (mode == Mode::DEBUG_STATE) {
        if (selected == DEBUG_STATE_BACK) {
            enterMode(Mode::DEBUG);
            return;
        }

        if (selected == DEBUG_STATE_STAGE) {
            debugStageIndex = (debugStageIndex + 1) % 5;
            Stage nextStage = (Stage)debugStageIndex;
            bug.debugSetStage(nextStage, GameEngine::ins().getGameNow());
            GameEngine::ins().clearTerrariumViewState();
            if (careItemsNotNeeded(nextStage)) {
                GameEngine::ins().setBowlStyle(0xFF);
                bug.setFoodTray(0, (FoodType)GameEngine::ins().getFoodStyle());
                GameEngine::ins().setWoodStyle(0xFF);
                bug.removeWood();
                saveSettingsNow();
            }
            GameEngine::ins().forceSave();

            Serial.printf("[Menu] Debug stage cycled to %d\n", debugStageIndex + 1);
        } else if (selected == DEBUG_STATE_TEMPER) {
            debugTemperIndex = (debugTemperIndex + 1) % 6;
            Temperament nextTemper = (Temperament)debugTemperIndex;
            bug.debugSetTemperament(nextTemper);
            GameEngine::ins().forceSave();

            char toast[32];
            const char* temper = itemLabel(selected, toast, sizeof(toast));
            showToast(temper);
            Serial.printf("[Menu] Debug temperament cycled to %d\n", debugTemperIndex);
        }
        return;
    }

    if (mode == Mode::DEBUG_ATTR) {
        if (selected == DEBUG_ATTR_BACK) {
            enterMode(Mode::DEBUG);
        } else if (selected >= DEBUG_ATTR_SIZ && selected <= DEBUG_ATTR_SPI) {
            openAttrEdit((uint8_t)selected);
        }
        return;
    }

    switch (selected) {
        case BOX:
            enterMode(Mode::BOX);
            break;
        case SOCIAL:
            if (GameEngine::ins().hasActiveVisitSession()) {
                showToast(UiStrings::VISIT_SOCIAL_LOCKED);
                break;
            }
            enterMode(Mode::SOCIAL);
            break;
        case EXPLORE: {
            enterMode(Mode::EXPLORE);
            break;
        }
        case SETTINGS:
            nextScene = SCENE_SETTINGS;
            break;
        case DEBUG:
            enterMode(Mode::DEBUG);
            break;
        case INFO:
            nextScene = SCENE_INFO;
            break;
        case BACK:
            nextScene = SCENE_TERRARIUM;
            break;
    }
}

