#include "CupScene.h"
#include "../core/GameEngine.h"

// 前向声明：从 PROGMEM 读取 NPC 名字
static const char* npcNameForIndex(uint8_t idx);
static const char* npcBugNameForIndex(uint8_t idx);
#include "../core/UiStrings.h"
#include "../hardware/Hal.h"
#include "../hardware/PixelRenderer.h"
#include "../assets/NpcData.h"
#include "../game/Bug.h"
#include <cmath>
#include <cstring>

uint8_t CupScene::sRound = 0;
NpcCombatant CupScene::sOpponents[7];
uint8_t CupScene::sFinalRank = 0;
bool CupScene::sInitialized = false;
bool CupScene::sParticipatedThisSeason = false;
bool CupScene::sBeatLegend[3] = {false, false, false};

void CupScene::onEnter() {
    stateStartMs = Hal::ins().millis();
    if (!sInitialized) {
        // 只有报名开放期才能进入；否则按未参赛结算
        if (GameEngine::ins().getCupCycleState() != GameEngine::CupCycleState::REGISTER_OPEN) {
            sInitialized = true;
            finishCup(0);
            Serial.printf("[Cup] Enter but registration closed, finishCup(0)\n");
            return;
        }
        initCup();
        sInitialized = true;
        sRound = 0;
        sFinalRank = 0;
        sParticipatedThisSeason = false;
        state = State::NOTIFY;
    } else {
        // 从 BattleScene 返回
        if (state == State::WAIT_BATTLE) {
            PendingNpcBattle& pending = GameEngine::ins().pendingNpcBattle();
            if (pending.resultSet) {
                advanceToNextRound(pending.won);
                pending.resultSet = false;
            }
        }
    }
    Serial.printf("[Cup] Enter state=%d round=%d\n", (int)state, sRound);
}

void CupScene::onExit() {
    if (nextScene != SCENE_CUP && nextScene != SCENE_BATTLE) {
        // 真正离开杯赛：清理状态
        sInitialized = false;
    }
}

void CupScene::initCup() {
    Bug& bug = GameEngine::ins().getBug();
    for (int i = 0; i < 7; i++) {
        sOpponents[i] = NpcGenerator::generateForCup(bug);
    }
    for (int i = 0; i < 3; i++) sBeatLegend[i] = false;
    // 赛季已在引擎 checkCupCycle 中递增，直接使用当前赛季
    uint16_t season = GameEngine::ins().getCupSeason();
    SaveManager::ins().saveCupGlobal(season, GameEngine::ins().getLastCupGameTime(),
                                     (uint8_t)GameEngine::ins().getCupCycleState());
}

SceneID CupScene::update() {
    // 报名通知态：若超过报名截止时间则自动放弃
    if (state == State::NOTIFY) {
        uint64_t deadline = (uint64_t)GameEngine::ins().getLastCupGameTime() * 1000ULL
                          + GameEngine::CUP_REGISTER_MS;
        if (GameEngine::ins().getGameNow() >= deadline) {
            finishCup(0);
        }
    }

    return nextScene;
}

bool CupScene::onButton(const ButtonEvent& ev) {
    if (ev.action != BtnAction::PRESSED) return false;

    if (state == State::NOTIFY) {
        if (ev.btn == 0) {
            // 参赛：标记本赛季为进行中并持久化
            sParticipatedThisSeason = true;
            GameEngine::ins().setCupCycleState(GameEngine::CupCycleState::IN_PROGRESS);
            SaveManager::ins().saveCupGlobal(GameEngine::ins().getCupSeason(),
                                             GameEngine::ins().getLastCupGameTime(),
                                             (uint8_t)GameEngine::CupCycleState::IN_PROGRESS);
            Bug& bug = GameEngine::ins().getBug();
            bug.recordCupParticipation();
            // 连续参赛 +1
            bug.incrementCupStreak();
            state = State::BRACKET;
            stateStartMs = Hal::ins().millis();
            bracketScroll = 0.0f;
            bracketScrollLastMs = stateStartMs;
            return true;
        }
        if (ev.btn == 1) {
            // 放弃
            finishCup(0);
            return true;
        }
    }

    if (state == State::BRACKET) {
        if (ev.btn == 0) {
            state = State::ROUND_INTRO;
            stateStartMs = Hal::ins().millis();
            return true;
        }
    }

    if (state == State::ROUND_INTRO) {
        if (ev.btn == 0) {
            // 进入对战
            NpcCombatant& npc = sOpponents[sRound];
            GameEngine::ins().setPendingNpcBattle(npc, SCENE_CUP, npc.tier == NpcData::Tier::LEGEND, false, true);
            state = State::WAIT_BATTLE;
            nextScene = SCENE_BATTLE;
            return true;
        }
    }

    if (state == State::RESULT) {
        if (ev.btn == 0 || ev.btn == 1) {
            nextScene = SCENE_TERRARIUM;
            return true;
        }
    }

    return false;
}

void CupScene::advanceToNextRound(bool won) {
    if (won && !GameEngine::ins().isFeatureUnlocked(GameEngine::FEATURE_TOY_BALL_UNLOCKED)) {
        GameEngine::ins().unlockFeature(GameEngine::FEATURE_TOY_BALL_UNLOCKED);
        GameEngine::ins().addToyBall(1);
        Serial.println("[Cup] Ball unlocked by first cup round win");
    }
    if (won && sRound < 3 && sOpponents[sRound].tier == NpcData::Tier::LEGEND) {
        sBeatLegend[sRound] = true;
    }
    if (!won) {
        // 淘汰：根据当前轮次决定名次
        uint8_t rank = 8;
        if (sRound == 1) rank = 4;
        else if (sRound == 2) rank = 2;
        finishCup(rank);
        return;
    }
    if (sRound >= 2) {
        finishCup(1); // 冠军
        return;
    }
    sRound++;
    state = State::ROUND_INTRO;
    stateStartMs = Hal::ins().millis();
}

void CupScene::finishCup(uint8_t rank) {
    sFinalRank = rank;
    Bug& bug = GameEngine::ins().getBug();
    bool firstChampion = rank == 1 && bug.getCupWins() == 0;
    if (rank > 0) {
        bug.recordCupResult(rank);
        if (rank == 1) {
            bug.recordCupWin();
            bug.setAchievementFlag(1 << 1); // 冠军之路
            if (firstChampion && GameEngine::ins().unlockMainSceneBg(GameEngine::BG_ENTOMOLOGIST)) {
                Serial.println("[Cup] Lab background unlocked by first champion");
            }
        }
        if (sBeatLegend[0] || sBeatLegend[1] || sBeatLegend[2]) {
            bug.recordCupLegendKill();
            bug.setAchievementFlag(1 << 2); // 传说猎手
        }
        bug.setAchievementFlag(1 << 0); // 初战告捷
        if (bug.getCupStreak() >= 5) {
            bug.setAchievementFlag(1 << 4); // 全勤奖
        }
    } else {
        bug.resetCupStreak();
    }
    applyCupRewards();
    // 本赛季结束，关闭报名窗口直到下一周期
    GameEngine::ins().setCupCycleState(GameEngine::CupCycleState::REGISTER_EXPIRED);
    SaveManager::ins().saveCupGlobal(GameEngine::ins().getCupSeason(),
                                     GameEngine::ins().getLastCupGameTime(),
                                     (uint8_t)GameEngine::CupCycleState::REGISTER_EXPIRED);
    GameEngine::ins().forceSave();
    state = State::RESULT;
    stateStartMs = Hal::ins().millis();
}

void CupScene::applyCupRewards() {
    Bug& bug = GameEngine::ins().getBug();
    switch (sFinalRank) {
        case 1:
            bug.addFood(FoodType::DROP, 6);
            bug.addRottenWood(1);
            // bug.modSpi(2.0f);
            bug.modHunger(100); // MOT 拉满简化：直接回满饥饿
            break;
        case 2:
            bug.addFood(FoodType::DROP, 4);
            // bug.modMot(50);
            break;
        case 4:
            bug.addFood(FoodType::DROP, 2);
            break;
        case 8:
            bug.addFood(FoodType::DROP, 1);
            break;
        default:
            break;
    }
}

void CupScene::render() {
    PixelRenderer::fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::rgb565(0, 0, 0));
    switch (state) {
        case State::NOTIFY:  drawNotify(); break;
        case State::BRACKET: drawBracket(); break;
        case State::ROUND_INTRO: drawRoundIntro(); break;
        case State::RESULT:  drawResult(); break;
        default: break;
    }
}

void CupScene::drawNotify() {
    LGFX_Sprite& canvas = Hal::ins().canvas();
    float fs = PixelRenderer::getContentFontScale();
    int cx = Hal::DISPLAY_W / 2;
    PixelRenderer::applyTextStyle(fs);

    char buf[32];
    snprintf(buf, sizeof(buf), UiStrings::CUP_SEASON_TITLE, GameEngine::ins().getCupSeason());
    int tw = canvas.textWidth(buf);
    PixelRenderer::drawPixelText(cx - tw / 2, 30, buf, PixelRenderer::YELLOW, fs);

    const char* title = UiStrings::CUP_STARTING;
    tw = canvas.textWidth(title);
    PixelRenderer::drawPixelText(cx - tw / 2, 54, title, PixelRenderer::WHITE, fs);

    tw = canvas.textWidth(UiStrings::CUP_NAV_JOIN_QUIT);
    PixelRenderer::drawPixelText(cx - tw / 2, 100, UiStrings::CUP_NAV_JOIN_QUIT, PixelRenderer::GRAY, fs);
}

void CupScene::drawBracket() {
    LGFX_Sprite& canvas = Hal::ins().canvas();
    float fs = PixelRenderer::getContentFontScale();
    int cx = Hal::DISPLAY_W / 2;
    PixelRenderer::applyTextStyle(fs);

    const char* title = UiStrings::CUP_BRACKET;
    int tw = canvas.textWidth(title);
    PixelRenderer::drawPixelText(cx - tw / 2, 8, title, PixelRenderer::YELLOW, fs);

    static constexpr int VIEW_X = 6;
    static constexpr int VIEW_Y = 24;
    static constexpr int VIEW_W = Hal::DISPLAY_W - VIEW_X * 2;
    static constexpr int VIEW_H = 78;
    static constexpr int CONTENT_W = 386;
    static constexpr int COL_QF_X = 8;
    static constexpr int COL_SF_X = 174;
    static constexpr int COL_FINAL_X = 300;
    const int rowY[4] = {8, 24, 44, 60};
    static constexpr int TEXT_GAP = 4;

    int maxScroll = CONTENT_W > VIEW_W ? CONTENT_W - VIEW_W : 0;
    updateBracketScroll(maxScroll);

    PixelRenderer::fillRect(VIEW_X, VIEW_Y, VIEW_W, VIEW_H, PixelRenderer::rgb565(8, 8, 12));
    canvas.drawRect(VIEW_X, VIEW_Y, VIEW_W, VIEW_H, PixelRenderer::GRAY);
    canvas.setClipRect(VIEW_X + 1, VIEW_Y + 1, VIEW_W - 2, VIEW_H - 2);

    int ox = VIEW_X + 4 - (int)bracketScroll;
    int oy = VIEW_Y + 5;
    PixelRenderer::drawPixelText(ox + COL_QF_X, oy - 1, UiStrings::CUP_ROUND_QUARTER,
                                 PixelRenderer::CYAN, fs);
    PixelRenderer::drawPixelText(ox + COL_SF_X, oy - 1, UiStrings::CUP_ROUND_SEMI,
                                 PixelRenderer::CYAN, fs);
    PixelRenderer::drawPixelText(ox + COL_FINAL_X, oy - 1, UiStrings::CUP_ROUND_FINAL,
                                 PixelRenderer::CYAN, fs);

    char buf[48];
    // 玩家固定 A1
    snprintf(buf, sizeof(buf), UiStrings::CUP_YOU_VS, npcNameForIndex(sOpponents[0].index));
    PixelRenderer::drawPixelText(ox + COL_QF_X, oy + rowY[0] + TEXT_GAP, buf,
                                 sRound == 0 ? PixelRenderer::YELLOW : PixelRenderer::WHITE, fs);

    for (int i = 1; i < 4; i++) {
        snprintf(buf, sizeof(buf), "%s%s%s",
                 npcNameForIndex(sOpponents[i * 2 - 1].index),
                 UiStrings::CUP_VS,
                 npcNameForIndex(sOpponents[i * 2].index));
        PixelRenderer::drawPixelText(ox + COL_QF_X, oy + rowY[i] + TEXT_GAP, buf, PixelRenderer::GRAY, fs);
    }

    uint16_t lineColor = PixelRenderer::rgb565(80, 92, 118);
    for (int pair = 0; pair < 2; ++pair) {
        int y1 = oy + rowY[pair * 2] + 9;
        int y2 = oy + rowY[pair * 2 + 1] + 9;
        int midY = (y1 + y2) / 2;
        int qRight = ox + COL_QF_X + 128;
        int sfLeft = ox + COL_SF_X - 10;
        canvas.drawFastHLine(qRight, y1, 18, lineColor);
        canvas.drawFastHLine(qRight, y2, 18, lineColor);
        canvas.drawFastVLine(qRight + 18, y1, y2 - y1 + 1, lineColor);
        int span = sfLeft - (qRight + 18);
        if (span > 0) canvas.drawFastHLine(qRight + 18, midY, span, lineColor);
    }

    int sf1Y = oy + 20;
    int sf2Y = oy + 52;
    int finalY = oy + 36;
    PixelRenderer::drawPixelText(ox + COL_SF_X, sf1Y, sRound >= 1 ? "Winner A" : "...",
                                 sRound == 1 ? PixelRenderer::YELLOW : PixelRenderer::WHITE, fs);
    PixelRenderer::drawPixelText(ox + COL_SF_X, sf2Y, sRound >= 1 ? "Winner B" : "...",
                                 sRound == 1 ? PixelRenderer::YELLOW : PixelRenderer::WHITE, fs);
    int sfRight = ox + COL_SF_X + 82;
    int finalLeft = ox + COL_FINAL_X - 8;
    canvas.drawFastHLine(sfRight, sf1Y + 5, 16, lineColor);
    canvas.drawFastHLine(sfRight, sf2Y + 5, 16, lineColor);
    canvas.drawFastVLine(sfRight + 16, sf1Y + 5, sf2Y - sf1Y + 1, lineColor);
    int finalSpan = finalLeft - (sfRight + 16);
    if (finalSpan > 0) canvas.drawFastHLine(sfRight + 16, finalY + 5, finalSpan, lineColor);
    PixelRenderer::drawPixelText(ox + COL_FINAL_X, finalY, sRound >= 2 ? "Finalist" : "...",
                                 sRound == 2 ? PixelRenderer::YELLOW : PixelRenderer::WHITE, fs);

    canvas.clearClipRect();
    if (bracketScroll > 1.0f) {
        PixelRenderer::drawPixelText(VIEW_X + 5, VIEW_Y + VIEW_H - 12, "<", PixelRenderer::GRAY, fs);
    }
    if (bracketScroll < (float)maxScroll - 1.0f) {
        PixelRenderer::drawPixelText(VIEW_X + VIEW_W - 12, VIEW_Y + VIEW_H - 12, ">",
                                     PixelRenderer::GRAY, fs);
    }

    tw = canvas.textWidth(UiStrings::CUP_START_ROUND);
    PixelRenderer::drawPixelText(cx - tw / 2, 110, UiStrings::CUP_START_ROUND, PixelRenderer::GREEN, fs);
}

void CupScene::updateBracketScroll(int maxScroll) {
    if (maxScroll <= 0) {
        bracketScroll = 0.0f;
        bracketScrollLastMs = Hal::ins().millis();
        return;
    }

    uint32_t now = Hal::ins().millis();
    if (bracketScrollLastMs == 0) bracketScrollLastMs = now;
    uint32_t dt = now - bracketScrollLastMs;
    if (dt > 120) dt = 120;
    bracketScrollLastMs = now;

    float ax, ay, az;
    Hal::ins().getAccel(ax, ay, az);
    float mag = Hal::ins().getAccelMagnitude();
    if (fabsf(ay) > 1.5f || mag > 2.0f) return;

    static constexpr float TILT_DEADZONE_G = 0.22f;
    if (fabsf(ax) <= TILT_DEADZONE_G) return;

    float strength = (fabsf(ax) - TILT_DEADZONE_G) / 0.55f;
    if (strength > 1.0f) strength = 1.0f;

    float dir = (ax < 0.0f) ? 1.0f : -1.0f;
    bracketScroll += dir * strength * 72.0f * ((float)dt / 1000.0f);
    if (bracketScroll < 0.0f) bracketScroll = 0.0f;
    if (bracketScroll > (float)maxScroll) bracketScroll = (float)maxScroll;
}

void CupScene::drawRoundIntro() {
    LGFX_Sprite& canvas = Hal::ins().canvas();
    float fs = PixelRenderer::getContentFontScale();
    int cx = Hal::DISPLAY_W / 2;
    PixelRenderer::applyTextStyle(fs);

    const char* roundName[3] = {UiStrings::CUP_ROUND_QUARTER,
                                UiStrings::CUP_ROUND_SEMI,
                                UiStrings::CUP_ROUND_FINAL};
    char buf[32];
    snprintf(buf, sizeof(buf), "%s", roundName[sRound]);
    int tw = canvas.textWidth(buf);
    PixelRenderer::drawPixelText(cx - tw / 2, 30, buf, PixelRenderer::YELLOW, fs);

    NpcCombatant& npc = sOpponents[sRound];
    snprintf(buf, sizeof(buf), UiStrings::CUP_OPPONENT, npcNameForIndex(npc.index));
    tw = canvas.textWidth(buf);
    PixelRenderer::drawPixelText(cx - tw / 2, 56, buf, PixelRenderer::WHITE, fs);

    snprintf(buf, sizeof(buf), UiStrings::CUP_BEETLE_LABEL, npcBugNameForIndex(npc.index));
    tw = canvas.textWidth(buf);
    PixelRenderer::drawPixelText(cx - tw / 2, 74, buf, PixelRenderer::CYAN, fs);

    tw = canvas.textWidth(UiStrings::CUP_BATTLE);
    PixelRenderer::drawPixelText(cx - tw / 2, 110, UiStrings::CUP_BATTLE, PixelRenderer::GREEN, fs);
}

void CupScene::drawResult() {
    LGFX_Sprite& canvas = Hal::ins().canvas();
    float fs = PixelRenderer::getContentFontScale();
    int cx = Hal::DISPLAY_W / 2;
    PixelRenderer::applyTextStyle(fs);

    const char* rt = rankText(sFinalRank);
    char buf[32];
    snprintf(buf, sizeof(buf), "%s", rt);
    int tw = canvas.textWidth(buf);
    PixelRenderer::drawPixelText(cx - tw / 2, 36, buf, PixelRenderer::YELLOW, fs);

    const char* reward = UiStrings::CUP_REWARD_ISSUED;
    if (sFinalRank == 0) reward = UiStrings::CUP_DID_NOT_JOIN;
    tw = canvas.textWidth(reward);
    PixelRenderer::drawPixelText(cx - tw / 2, 64, reward, PixelRenderer::WHITE, fs);

    tw = canvas.textWidth(UiStrings::CUP_BACK);
    PixelRenderer::drawPixelText(cx - tw / 2, 110, UiStrings::CUP_BACK, PixelRenderer::GRAY, fs);
}

const char* CupScene::rankText(uint8_t rank) const {
    switch (rank) {
        case 1: return UiStrings::CUP_CHAMPION;
        case 2: return UiStrings::CUP_RUNNER_UP;
        case 4: return UiStrings::CUP_TOP_FOUR;
        case 8: return UiStrings::CUP_TOP_EIGHT;
        default: return UiStrings::CUP_DID_NOT_JOIN;
    }
}

// 静态辅助：从 PROGMEM 读取 NPC 名字（临时 buffer 在栈上，调用方需立即使用）
static char sNameBuf[16];
static const char* npcNameForIndex(uint8_t idx) {
    strncpy_P(sNameBuf, (const char*)pgm_read_ptr(&NpcData::ENTRIES[idx].name), sizeof(sNameBuf) - 1);
    sNameBuf[sizeof(sNameBuf) - 1] = '\0';
    return sNameBuf;
}
static char sBugNameBuf[16];
static const char* npcBugNameForIndex(uint8_t idx) {
    strncpy_P(sBugNameBuf, (const char*)pgm_read_ptr(&NpcData::ENTRIES[idx].bugName), sizeof(sBugNameBuf) - 1);
    sBugNameBuf[sizeof(sBugNameBuf) - 1] = '\0';
    return sBugNameBuf;
}
