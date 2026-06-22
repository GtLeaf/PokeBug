#include "ExploreScene.h"
#include "../core/GameEngine.h"
#include "../core/UiStrings.h"
#include "../hardware/Hal.h"
#include "../hardware/PixelRenderer.h"
#include "../assets/ExploreAssets.h"
#include <cstring>

bool ExploreScene::sSessionActive = false;
uint8_t ExploreScene::sCurrentRound = 1;
int ExploreScene::sTotalSapGain = 0;
int ExploreScene::sTotalWoodGain = 0;
bool ExploreScene::sFinalRecorded = false;

namespace {

static constexpr uint32_t EXPLORE_EVENT_DELAY_MS = 2000;
static constexpr uint32_t EXPLORE_REVEAL_SHAKE_MS = 360;
static constexpr int EXPLORE_REVEAL_SHAKE_PX = 2;

struct ExploreEventWeights {
    uint8_t sap;
    uint8_t food;
    uint8_t wood;
    uint8_t npc;
    uint8_t rare;
    uint8_t nothing;
};

static constexpr ExploreEventWeights EVENT_TABLE[GameEngine::EXPLORE_LOCATION_COUNT][3] = {
    { // Park
        {40, 15, 10, 10, 5, 20},
        {30, 20, 10, 15, 8, 17},
        {25, 15, 10, 20, 10, 20},
    },
    { // Back Hill
        {20, 10, 30, 10, 10, 20},
        {30, 15, 20, 10, 10, 15},
        {15, 10, 20, 25, 10, 20},
    },
    { // Riverside
        {15, 15, 10, 15, 30, 15},
        {25, 20, 10, 15, 15, 15},
        {10, 10, 10, 20, 25, 25},
    },
    { // Old Woods
        {10, 10, 15, 20, 25, 20},
        {10, 10, 15, 25, 20, 20},
        {5, 5, 10, 35, 20, 25},
    },
};

static constexpr uint8_t NPC_TIER_TABLE[GameEngine::EXPLORE_LOCATION_COUNT][3][4] = {
    { // Park
        {70, 25, 5, 0},
        {70, 25, 5, 0},
        {70, 25, 5, 0},
    },
    { // Back Hill
        {40, 45, 15, 0},
        {40, 45, 14, 1},
        {35, 42, 20, 3},
    },
    { // Riverside
        {20, 40, 30, 10},
        {20, 40, 30, 10},
        {15, 35, 35, 15},
    },
    { // Old Woods
        {10, 30, 40, 20},
        {10, 30, 40, 20},
        {5, 25, 40, 30},
    },
};

uint8_t rgb565R(uint16_t color) {
    return (uint8_t)((color >> 8) & 0xF8);
}

uint8_t rgb565G(uint16_t color) {
    return (uint8_t)((color >> 3) & 0xFC);
}

uint8_t rgb565B(uint16_t color) {
    return (uint8_t)((color << 3) & 0xF8);
}

uint16_t blendRgb565(uint16_t dst, uint16_t src, uint8_t alpha) {
    uint8_t inv = 255 - alpha;
    uint8_t r = (uint8_t)((rgb565R(src) * alpha + rgb565R(dst) * inv) / 255);
    uint8_t g = (uint8_t)((rgb565G(src) * alpha + rgb565G(dst) * inv) / 255);
    uint8_t b = (uint8_t)((rgb565B(src) * alpha + rgb565B(dst) * inv) / 255);
    return PixelRenderer::rgb565(r, g, b);
}

void fillRectAlpha(int x, int y, int w, int h, uint16_t color, uint8_t alpha) {
    LGFX_Sprite& canvas = Hal::ins().canvas();
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > Hal::DISPLAY_W ? Hal::DISPLAY_W : x + w;
    int y1 = y + h > Hal::DISPLAY_H ? Hal::DISPLAY_H : y + h;
    for (int py = y0; py < y1; ++py) {
        for (int px = x0; px < x1; ++px) {
            canvas.drawPixel(px, py, blendRgb565(canvas.readPixel(px, py), color, alpha));
        }
    }
}

}

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
    totalWoodGain = 0;
    finalRecorded = false;
    saveSession();
}

void ExploreScene::restoreSession() {
    if (!sSessionActive) {
        startNewSession();
        return;
    }
    currentRound = sCurrentRound;
    totalSapGain = sTotalSapGain;
    totalWoodGain = sTotalWoodGain;
    finalRecorded = sFinalRecorded;
}

void ExploreScene::saveSession() {
    sSessionActive = true;
    sCurrentRound = currentRound;
    sTotalSapGain = totalSapGain;
    sTotalWoodGain = totalWoodGain;
    sFinalRecorded = finalRecorded;
}

void ExploreScene::clearSession() {
    sSessionActive = false;
    sCurrentRound = 1;
    sTotalSapGain = 0;
    sTotalWoodGain = 0;
    sFinalRecorded = false;
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
    } else {
        eventType = EventType::NOTHING;
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

    switch (eventType) {
        case EventType::SAP:
            bug.addFood(FoodType::DROP, eventValue);
            totalSapGain += eventValue;
            snprintf(resultLine1, sizeof(resultLine1), UiStrings::EXPLORE_SAP_PLUS, eventValue);
            break;
        case EventType::FOOD_SOURCE:
            bug.addFood(FoodType::DROP, eventValue);
            totalSapGain += eventValue;
            snprintf(resultLine1, sizeof(resultLine1), UiStrings::EXPLORE_FOOD_SOURCE_PLUS, eventValue);
            break;
        case EventType::WOOD:
            if (bug.getRottenWood() == 0 && !bug.isWoodPlaced()) {
                bug.addRottenWood(1);
                totalWoodGain += 1;
                snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_ROTTEN_WOOD_PLUS);
            } else {
                bug.addFood(FoodType::DROP, 1);
                totalSapGain += 1;
                snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_WOOD_FULL_SAP);
            }
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
            auto addWood = [&](uint8_t amount) {
                bug.addRottenWood(amount);
                totalWoodGain += amount;
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
                            addSap(sap);
                            snprintf(resultLine1, sizeof(resultLine1), UiStrings::EXPLORE_PICNIC_CRUMBS, sap);
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
                            addStat(0.0f, 0.0f, 0.0f, 0.0f, 0.1f, "SPI +0.1");
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
                            addWood(1);
                            addSap(1);
                            snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_SUN_BARK);
                            break;
                        case 1:
                            addSap(2);
                            snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_ANT_TRAIL);
                            break;
                        case 2:
                            addSap(1);
                            snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_PINECONE);
                            break;
                        case 3:
                            addWood(1);
                            addSap(2);
                            snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_CRACKED_STUMP);
                            break;
                        case 4:
                            addSap(2);
                            addStat(0.0f, 0.0f, 0.1f, 0.0f, 0.0f, "END +0.1");
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
                            addWood(1);
                            snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_MOSS_CARPET);
                            break;
                        case 1:
                            addSap(3);
                            addStat(0.0f, 0.0f, 0.0f, 0.0f, 0.2f, "SPI +0.2");
                            snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_FIREFLIES);
                            break;
                        case 2:
                            addSap(1);
                            snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_BULLFROG);
                            break;
                        case 3:
                            addSap(3);
                            addWood(1);
                            addStat(0.0f, 0.0f, 0.2f, 0.0f, 0.0f, "END +0.2");
                            snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_FAIRY_RING);
                            break;
                        case 4:
                            addSap(2);
                            addWood(1);
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
                            addStat(0.0f, 0.0f, 0.3f, 0.0f, 0.0f, "END +0.3");
                            snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_ANCIENT_RESIN);
                            break;
                        case 1:
                            addSap(2);
                            addWood(1);
                            snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_DEER_MUSHROOM);
                            break;
                        case 2:
                            addSap(3);
                            addWood(1);
                            snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_DEADWOOD_BEETLES);
                            break;
                        case 3:
                            addSap(5);
                            snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_GLOW_MOSS);
                            break;
                        case 4:
                            addSap(2);
                            addWood(1);
                            addStat(0.0f, 0.2f, 0.0f, 0.0f, 0.0f, "STR +0.2");
                            snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_HEART_OF_ROT);
                            break;
                        case 5:
                        default:
                            addSap(6);
                            addWood(1);
                            addStat(0.0f, 0.0f, 0.0f, 0.0f, 0.3f, "SPI +0.3");
                            snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_OLD_PHANTOM);
                            break;
                    }
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
    } else if (totalWoodGain > 0) {
        snprintf(resultLine2, sizeof(resultLine2), "Sap +%d Wood +%d", totalSapGain, totalWoodGain);
    } else {
        snprintf(resultLine2, sizeof(resultLine2), "Sap +%d", totalSapGain);
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
    if (res.won) {
        int sap = 0;
        switch (res.tier) {
            case NpcData::Tier::ROOKIE:  sap = 1; break;
            case NpcData::Tier::NORMAL:  sap = random(1, 3); break;
            case NpcData::Tier::VETERAN: sap = random(2, 4); break;
            case NpcData::Tier::LEGEND:  sap = random(3, 6); bug.addRottenWood(1); break;
            default: break;
        }
        bug.addFood(FoodType::DROP, sap);
        totalSapGain += sap;
        if (res.tier == NpcData::Tier::LEGEND) totalWoodGain += 1;
        snprintf(resultLine1, sizeof(resultLine1), UiStrings::EXPLORE_VICTORY_SAP, sap);
        snprintf(resultLine2, sizeof(resultLine2), "SPI +0.5");
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
                snprintf(lossBuf, sizeof(lossBuf), "-%d 树汁", sapLoss);
            }
        }
        snprintf(resultLine1, sizeof(resultLine1), "%s", UiStrings::EXPLORE_DEFEATED);
        snprintf(resultLine2, sizeof(resultLine2), "%s", lossBuf);
        bug.setMot(0);
        enterFinalSummary(UiStrings::EXPLORE_DEFEATED, UiStrings::EXPLORE_MOT_ZERO);
    }
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
        case State::NPC_PROMPT:
            drawNpcPrompt();
            break;
        default:
            break;
    }
}

void ExploreScene::drawSkyAndGround(int shakeX) {
    LGFX_Sprite& canvas = Hal::ins().canvas();
    const uint16_t* bg = ExploreAssets::background(GameEngine::ins().getExploreLocation(),
                                                   GameEngine::ins().getTimeOfDay());
    if (bg) {
        PixelRenderer::drawRgb565(shakeX, 0,
                                  ExploreAssets::FRAME_W,
                                  ExploreAssets::FRAME_H,
                                  bg);
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
    canvas.setTextSize(fs);
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
    canvas.setTextSize(fs);
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
    const char* nav = (state == State::FINAL_SUMMARY) ? "A:Return" : UiStrings::EXPLORE_NEXT_RETURN;
    tw = canvas.textWidth(nav);
    PixelRenderer::drawPixelText(cx - tw / 2, Hal::DISPLAY_H - 44, nav, PixelRenderer::GRAY, fs);
}

void ExploreScene::drawNpcPrompt() {
    LGFX_Sprite& canvas = Hal::ins().canvas();
    float fs = PixelRenderer::getContentFontScale();
    drawExploring(0, false);
    fillRectAlpha(16, 24, Hal::DISPLAY_W - 32, Hal::DISPLAY_H - 48, PixelRenderer::rgb565(24, 24, 24), 190);
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

    PixelRenderer::drawPixelText(cx - 58, Hal::DISPLAY_H - 36, UiStrings::EXPLORE_ACCEPT, PixelRenderer::GREEN, fs);
    PixelRenderer::drawPixelText(cx + 20, Hal::DISPLAY_H - 36, UiStrings::EXPLORE_LEAVE, PixelRenderer::GRAY, fs);
}

void ExploreScene::drawResult() {
    drawPopup();
}
