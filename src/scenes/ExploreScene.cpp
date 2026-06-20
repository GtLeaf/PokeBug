#include "ExploreScene.h"
#include "../core/GameEngine.h"
#include "../core/UiStrings.h"
#include "../hardware/Hal.h"
#include "../hardware/PixelRenderer.h"
#include "../game/BattleCalc.h"
#include "../assets/HerculesAdultSprites.h"
#include <cstring>

void ExploreScene::onEnter() {
    Bug& bug = GameEngine::ins().getBug();

    // 检查是否刚从 NPC 对战返回
    NpcBattleResult& res = GameEngine::ins().lastNpcBattleResult();
    if (res.valid && res.fromExplore) {
        // 不重复扣饥饿度，应用 NPC 对战奖励/惩罚
        applyNpcBattleResult(res);
        GameEngine::ins().clearLastNpcBattleResult();
        state = State::RESULT;
        resultTimeoutMs = Hal::ins().millis() + 10000;
        exploreStartMs = Hal::ins().millis(); // 重置探索时间？还是保留？这里简化重置
        return;
    }

    // 首次进入：扣除饥饿度 20
    if (bug.getHunger() >= 30) {
        bug.modHunger(-20);
    }
    bugX = 120.0f;
    faceRight = true;
    state = State::EXPLORING;
    exploreStartMs = Hal::ins().millis();
    resultTimeoutMs = 0;
    nextEventMs = 0;
    eventType = EventType::NOTHING;
    resultLine1[0] = '\0';
    resultLine2[0] = '\0';
    resetEventTimer(exploreStartMs);
    Serial.println("[Explore] Enter");
}

void ExploreScene::onExit() {
    Serial.println("[Explore] Exit");
}

void ExploreScene::resetEventTimer(uint32_t now) {
    uint32_t interval = EVENT_INTERVAL_MIN_MS +
                        random(EVENT_INTERVAL_MAX_MS - EVENT_INTERVAL_MIN_MS + 1);
    nextEventMs = now + interval;
}

SceneID ExploreScene::update() {
    uint32_t now = Hal::ins().millis();

    if (state == State::EXPLORING) {
        // 倾斜控制移动
        Hal::ins().updateIMU();
        float ax, ay, az;
        Hal::ins().getAccel(ax, ay, az);
        if (ax > TILT_THRESHOLD) {
            faceRight = false;
            bugX -= 1.5f;
        } else if (ax < -TILT_THRESHOLD) {
            faceRight = true;
            bugX += 1.5f;
        }
        if (bugX < MIN_X) { bugX = MIN_X; faceRight = true; }
        if (bugX > MAX_X) { bugX = MAX_X; faceRight = false; }

        // 探索时间到或事件触发
        if (now >= nextEventMs) {
            triggerEvent(now);
        }
        if (now - exploreStartMs >= EXPLORE_DURATION_MS) {
            snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_END);
            snprintf(resultLine2, sizeof(resultLine2), "%s", UiStrings::EXPLORE_PRESS_A_RETURN);
            state = State::RESULT;
            resultTimeoutMs = now + 30000;
        }
    }

    if ((state == State::RESULT || state == State::EVENT_POPUP) &&
        resultTimeoutMs != 0 && now >= resultTimeoutMs) {
        // 超时自动继续/返回
        state = State::RETURNING;
    }

    if (state == State::RETURNING) {
        return SCENE_TERRARIUM;
    }

    return nextScene;
}

void ExploreScene::triggerEvent(uint32_t now) {
    Bug& bug = GameEngine::ins().getBug();
    int roll = random(100);
    if (roll < 28) {
        eventType = EventType::SAP;
        eventValue = random(1, 4); // 1-3
    } else if (roll < 53) {
        eventType = EventType::NPC;
    } else if (roll < 63) {
        eventType = EventType::WOOD;
    } else if (roll < 83) {
        eventType = EventType::FOOD_SOURCE;
        eventValue = random(1, 3); // 1-2
    } else if (roll < 90) {
        eventType = EventType::RARE;
        rareSubType = random(6);
    } else {
        eventType = EventType::NOTHING;
    }

    if (eventType == EventType::NPC) {
        npc = NpcGenerator::generateForExplore(bug);
        // 读取 PROGMEM 名字/台词
        strncpy_P(npcName, (const char*)pgm_read_ptr(&NpcData::ENTRIES[npc.index].name), sizeof(npcName) - 1);
        npcName[sizeof(npcName) - 1] = '\0';
        strncpy_P(npcBugName, (const char*)pgm_read_ptr(&NpcData::ENTRIES[npc.index].bugName), sizeof(npcBugName) - 1);
        npcBugName[sizeof(npcBugName) - 1] = '\0';
        strncpy_P(npcMeetLine, (const char*)pgm_read_ptr(&NpcData::ENTRIES[npc.index].meet), sizeof(npcMeetLine) - 1);
        npcMeetLine[sizeof(npcMeetLine) - 1] = '\0';
        canFlee = (npc.tier != NpcData::Tier::LEGEND);
        state = State::NPC_PROMPT;
        return;
    }

    applyEventReward();
    state = State::EVENT_POPUP;
    resultTimeoutMs = now + 15000;
}

void ExploreScene::applyEventReward(bool flee) {
    Bug& bug = GameEngine::ins().getBug();
    resultLine1[0] = '\0';
    resultLine2[0] = '\0';

    switch (eventType) {
        case EventType::SAP:
            bug.addFood(FoodType::DROP, eventValue);
            snprintf(resultLine1, sizeof(resultLine1), UiStrings::EXPLORE_SAP_PLUS, eventValue);
            break;
        case EventType::FOOD_SOURCE:
            bug.addFood(FoodType::DROP, eventValue);
            snprintf(resultLine1, sizeof(resultLine1), UiStrings::EXPLORE_FOOD_SOURCE_PLUS, eventValue);
            break;
        case EventType::WOOD:
            if (bug.getRottenWood() == 0 && !bug.isWoodPlaced()) {
                bug.addRottenWood(1);
                snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_ROTTEN_WOOD_PLUS);
            } else {
                bug.addFood(FoodType::DROP, 1);
                snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_WOOD_FULL_SAP);
            }
            break;
        case EventType::RARE: {
            switch (rareSubType) {
                case 0: // 金色树汁
                    bug.addFood(FoodType::DROP, 5);
                    snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_GOLDEN_SAP);
                    break;
                case 1: // 树脂结晶
                    bug.addFood(FoodType::DROP, 3);
                    // 简化：直接 END +0.5
                    // bug.modEnd(0.5f); // 稍后添加
                    snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_RESIN_CRYSTAL);
                    break;
                case 2: { // 蝴蝶引路
                    bug.addFood(FoodType::DROP, 1);
                    snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_BUTTERFLY_GUIDES);
                    // 立即再触发一次普通事件
                    state = State::EVENT_POPUP; // 先显示本条，下一帧再触发？
                    // 简化：直接给额外树汁
                    bug.addFood(FoodType::DROP, 1);
                    snprintf(resultLine2, sizeof(resultLine2), "%s", UiStrings::EXPLORE_EXTRA_SAP);
                    break;
                }
                case 3: // 雨后菌丛
                    bug.addFood(FoodType::DROP, 3);
                    bug.addRottenWood(1);
                    snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_MUSHROOMS);
                    break;
                case 4: // 蜜露喷泉
                    bug.addFood(FoodType::DROP, 6);
                    snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_HONEYDEW_SPRING);
                    break;
                case 5: // 幻影甲虫
                    bug.addFood(FoodType::DROP, 5);
                    bug.addRottenWood(1);
                    // bug.modSpi(0.5f);
                    snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_PHANTOM_BLESSING);
                    break;
            }
            break;
        }
        case EventType::NOTHING:
            snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_NOTHING_HAPPENED);
            break;
        default:
            break;
    }
}

void ExploreScene::startNpcBattle() {
    GameEngine::ins().setPendingNpcBattle(npc, SCENE_EXPLORE, npc.tier == NpcData::Tier::LEGEND, true, false);
    nextScene = SCENE_BATTLE;
}

void ExploreScene::applyNpcBattleResult(const NpcBattleResult& res) {
    Bug& bug = GameEngine::ins().getBug();
    if (res.won) {
        bug.onBattleEnd(true, GameEngine::ins().getGameNow());
        int sap = 0;
        float spi = 0.0f;
        switch (res.tier) {
            case NpcData::Tier::ROOKIE:  sap = 1; spi = 0.2f; break;
            case NpcData::Tier::NORMAL:  sap = random(1, 3); spi = 0.3f; break;
            case NpcData::Tier::VETERAN: sap = random(2, 4); spi = 0.5f; break;
            case NpcData::Tier::LEGEND:  sap = random(3, 6); spi = 1.0f; bug.addRottenWood(1); break;
            default: break;
        }
        bug.addFood(FoodType::DROP, sap);
        // SPI 增加（简化直接加，上限由 Bug 内部 clamp）
        // 由于 Bug::spi 私有且无 modSpi，这里通过 onBattleEnd 已处理 SPI
        snprintf(resultLine1, sizeof(resultLine1), UiStrings::EXPLORE_VICTORY_SAP, sap);
        snprintf(resultLine2, sizeof(resultLine2), "SPI +%.1f", spi);
    } else {
        bug.onBattleEnd(false, GameEngine::ins().getGameNow());
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
                snprintf(lossBuf, sizeof(lossBuf), "-%d 树汁", sapLoss);
            }
        }
        snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_DEFEATED);
        snprintf(resultLine2, sizeof(resultLine2), "%s", lossBuf);
    }
}

void ExploreScene::doRelease() {
    Bug& bug = GameEngine::ins().getBug();
    bug.release(GameEngine::ins().getGameNow());
    GameEngine::ins().forceSave();
    nextScene = SCENE_TERRARIUM;
}

bool ExploreScene::onButton(const ButtonEvent& ev) {
    uint32_t now = Hal::ins().millis();

    if (ev.action == BtnAction::LONG_PRESS) {
        if (ev.btn == 0 && state == State::EXPLORING) {
            state = State::RELEASE_CONFIRM;
            return true;
        }
        if (ev.btn == 1) {
            // 长按 B 中断探索
            state = State::RETURNING;
            return true;
        }
    }

    if (ev.action != BtnAction::PRESSED) return false;

    if (state == State::EXPLORING) {
        if (ev.btn == 1) {
            // 短按 B 中断
            state = State::RETURNING;
            return true;
        }
        return false;
    }

    if (state == State::EVENT_POPUP || state == State::RESULT) {
        if (ev.btn == 0) {
            state = State::EXPLORING;
            resetEventTimer(now);
            return true;
        }
        return false;
    }

    if (state == State::NPC_PROMPT) {
        if (ev.btn == 0) {
            startNpcBattle();
            return true;
        }
        if (ev.btn == 1 && canFlee) {
            // 逃跑不算败绩，无惩罚
            snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_FLED);
            snprintf(resultLine2, sizeof(resultLine2), "%s", UiStrings::EXPLORE_SAFE_RETURN);
            state = State::RESULT;
            resultTimeoutMs = now + 10000;
            return true;
        }
        return false;
    }

    if (state == State::RELEASE_CONFIRM) {
        if (ev.btn == 0) {
            doRelease();
            return true;
        }
        if (ev.btn == 1) {
            state = State::EXPLORING;
            return true;
        }
    }

    return false;
}

void ExploreScene::render() {
    PixelRenderer::fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::rgb565(0, 0, 0));

    switch (state) {
        case State::EXPLORING:
            drawExploring();
            break;
        case State::EVENT_POPUP:
        case State::RESULT:
            drawPopup();
            break;
        case State::NPC_PROMPT:
            drawNpcPrompt();
            break;
        case State::RELEASE_CONFIRM:
            drawReleaseConfirm();
            break;
        default:
            break;
    }
}

void ExploreScene::drawSkyAndGround() {
    LGFX_Sprite& canvas = Hal::ins().canvas();
    // 天空渐变：上蓝下浅
    for (int y = 0; y < BUG_Y + 10; y++) {
        uint8_t r = 0, g = (uint8_t)(40 + y / 3), b = (uint8_t)(60 + y / 4);
        canvas.drawFastHLine(0, y, Hal::DISPLAY_W, PixelRenderer::rgb565(r, g, b));
    }
    // 太阳
    canvas.fillCircle(Hal::DISPLAY_W - 24, 18, 6, PixelRenderer::YELLOW);
    // 草地
    PixelRenderer::fillRect(0, BUG_Y + 10, Hal::DISPLAY_W, Hal::DISPLAY_H - (BUG_Y + 10),
                            PixelRenderer::rgb565(0, 80, 0));
    // 草丛装饰
    for (int x = 10; x < Hal::DISPLAY_W; x += 45) {
        PixelRenderer::fillRect(x, BUG_Y + 4, 4, 10, PixelRenderer::rgb565(0, 100, 0));
        PixelRenderer::fillRect(x + 6, BUG_Y + 6, 4, 8, PixelRenderer::rgb565(0, 110, 0));
    }
}

void ExploreScene::drawBug(int x, int y, bool right, uint8_t palette) {
    // 简化为一个像素甲虫：椭圆身体 + 角
    LGFX_Sprite& canvas = Hal::ins().canvas();
    uint16_t bodyColor = PixelRenderer::BROWN;
    if (palette == 1) bodyColor = PixelRenderer::rgb565(220, 180, 0); // 金
    else if (palette == 2) bodyColor = PixelRenderer::rgb565(230, 200, 230); // 白化
    else if (palette == 3) bodyColor = PixelRenderer::rgb565(180, 0, 180); // 虹彩

    canvas.fillEllipse(x, y, 14, 8, bodyColor);
    // 角
    int hornX = right ? x + 10 : x - 10;
    canvas.drawLine(x, y - 4, hornX, y - 12, PixelRenderer::rgb565(200, 200, 200));
    // 腿
    for (int i = -1; i <= 1; i++) {
        int lx = x + i * 6;
        canvas.drawLine(lx, y + 4, lx + (right ? 4 : -4), y + 10, PixelRenderer::BLACK);
    }
}

void ExploreScene::drawExploring() {
    drawSkyAndGround();
    drawBug((int)bugX, BUG_Y, faceRight, GameEngine::ins().getBug().getPaletteId());

    float fs = PixelRenderer::getContentFontScale();
    char buf[32];
    uint32_t remain = (EXPLORE_DURATION_MS > (Hal::ins().millis() - exploreStartMs))
                          ? (EXPLORE_DURATION_MS - (Hal::ins().millis() - exploreStartMs)) / 1000
                          : 0;
    snprintf(buf, sizeof(buf), UiStrings::EXPLORE_TIME_REMAIN, remain);
    PixelRenderer::drawPixelText(8, 6, buf, PixelRenderer::WHITE, fs);
    PixelRenderer::drawPixelText(8, Hal::DISPLAY_H - 14, UiStrings::EXPLORE_NAV_BACK_RELEASE, PixelRenderer::GRAY, fs);
}

void ExploreScene::drawPopup() {
    LGFX_Sprite& canvas = Hal::ins().canvas();
    float fs = PixelRenderer::getContentFontScale();
    PixelRenderer::fillRect(20, 30, Hal::DISPLAY_W - 40, Hal::DISPLAY_H - 60, PixelRenderer::rgb565(40, 40, 40));
    canvas.drawRect(20, 30, Hal::DISPLAY_W - 40, Hal::DISPLAY_H - 60, PixelRenderer::WHITE);

    int cx = Hal::DISPLAY_W / 2;
    canvas.setTextSize(fs);
    int tw = canvas.textWidth(resultLine1);
    PixelRenderer::drawPixelText(cx - tw / 2, 44, resultLine1, PixelRenderer::WHITE, fs);
    if (resultLine2[0]) {
        tw = canvas.textWidth(resultLine2);
        PixelRenderer::drawPixelText(cx - tw / 2, 44 + (int)(10 * fs), resultLine2, PixelRenderer::CYAN, fs);
    }
    tw = canvas.textWidth(UiStrings::EXPLORE_CONTINUE);
    PixelRenderer::drawPixelText(cx - tw / 2, Hal::DISPLAY_H - 44, UiStrings::EXPLORE_CONTINUE, PixelRenderer::GRAY, fs);
}

void ExploreScene::drawNpcPrompt() {
    LGFX_Sprite& canvas = Hal::ins().canvas();
    float fs = PixelRenderer::getContentFontScale();
    PixelRenderer::fillRect(16, 24, Hal::DISPLAY_W - 32, Hal::DISPLAY_H - 48, PixelRenderer::rgb565(40, 40, 40));
    canvas.drawRect(16, 24, Hal::DISPLAY_W - 32, Hal::DISPLAY_H - 48, PixelRenderer::WHITE);

    int cx = Hal::DISPLAY_W / 2;
    canvas.setTextSize(fs);
    char buf[48];
    snprintf(buf, sizeof(buf), "[%s]%s", NpcData::tierName(npc.tier), npcName);
    int tw = canvas.textWidth(buf);
    PixelRenderer::drawPixelText(cx - tw / 2, 36, buf, PixelRenderer::YELLOW, fs);
    snprintf(buf, sizeof(buf), UiStrings::EXPLORE_BEETLE_LABEL, npcBugName);
    tw = canvas.textWidth(buf);
    PixelRenderer::drawPixelText(cx - tw / 2, 52, buf, PixelRenderer::WHITE, fs);
    tw = canvas.textWidth(npcMeetLine);
    PixelRenderer::drawPixelText(cx - tw / 2, 70, npcMeetLine, PixelRenderer::CYAN, fs);

    PixelRenderer::drawPixelText(cx - 50, Hal::DISPLAY_H - 36, UiStrings::EXPLORE_FIGHT, PixelRenderer::GREEN, fs);
    if (canFlee) {
        PixelRenderer::drawPixelText(cx + 20, Hal::DISPLAY_H - 36, UiStrings::EXPLORE_FLEE, PixelRenderer::GRAY, fs);
    } else {
        PixelRenderer::drawPixelText(cx + 20, Hal::DISPLAY_H - 36, UiStrings::EXPLORE_CANT_FLEE, PixelRenderer::RED, fs);
    }
}

void ExploreScene::drawReleaseConfirm() {
    LGFX_Sprite& canvas = Hal::ins().canvas();
    float fs = PixelRenderer::getContentFontScale();
    PixelRenderer::fillRect(20, 30, Hal::DISPLAY_W - 40, Hal::DISPLAY_H - 60, PixelRenderer::rgb565(40, 40, 40));
    canvas.drawRect(20, 30, Hal::DISPLAY_W - 40, Hal::DISPLAY_H - 60, PixelRenderer::WHITE);

    int cx = Hal::DISPLAY_W / 2;
    canvas.setTextSize(fs);
    int tw = canvas.textWidth(UiStrings::EXPLORE_RELEASE_CONFIRM);
    PixelRenderer::drawPixelText(cx - tw / 2, 44, UiStrings::EXPLORE_RELEASE_CONFIRM, PixelRenderer::WHITE, fs);
    tw = canvas.textWidth(UiStrings::EXPLORE_RELEASE_EGG);
    PixelRenderer::drawPixelText(cx - tw / 2, 64, UiStrings::EXPLORE_RELEASE_EGG, PixelRenderer::GRAY, fs);
    PixelRenderer::drawPixelText(cx - 50, Hal::DISPLAY_H - 44, UiStrings::EXPLORE_CONFIRM, PixelRenderer::GREEN, fs);
    PixelRenderer::drawPixelText(cx + 20, Hal::DISPLAY_H - 44, UiStrings::EXPLORE_CANCEL, PixelRenderer::GRAY, fs);
}

void ExploreScene::drawResult() {
    drawPopup();
}
