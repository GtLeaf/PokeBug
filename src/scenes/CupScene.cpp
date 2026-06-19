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
        GameEngine::ins().setCupPendingNotify(false);
    }
}

void CupScene::initCup() {
    Bug& bug = GameEngine::ins().getBug();
    for (int i = 0; i < 7; i++) {
        sOpponents[i] = NpcGenerator::generateForCup(bug);
    }
    for (int i = 0; i < 3; i++) sBeatLegend[i] = false;
    uint16_t season = GameEngine::ins().getCupSeason() + 1;
    GameEngine::ins().setCupSeason(season);
    GameEngine::ins().setLastCupTime((uint32_t)(Hal::ins().millis() / 1000ULL));
    SaveManager::ins().saveCupGlobal(season, GameEngine::ins().getLastCupTime());
}

SceneID CupScene::update() {
    uint32_t now = Hal::ins().millis();

    if (state == State::NOTIFY) {
        if (now - stateStartMs >= NOTIFY_TIMEOUT_MS) {
            // 自动放弃
            finishCup(0);
        }
    }

    return nextScene;
}

bool CupScene::onButton(const ButtonEvent& ev) {
    if (ev.action != BtnAction::PRESSED) return false;

    if (state == State::NOTIFY) {
        if (ev.btn == 0) {
            // 参赛
            sParticipatedThisSeason = true;
            Bug& bug = GameEngine::ins().getBug();
            bug.recordCupParticipation();
            // 连续参赛 +1
            bug.incrementCupStreak();
            state = State::BRACKET;
            stateStartMs = Hal::ins().millis();
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
    if (rank > 0) {
        bug.recordCupResult(rank);
        if (rank == 1) {
            bug.recordCupWin();
            bug.setAchievementFlag(1 << 1); // 冠军之路
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
    GameEngine::ins().setLastCupTime((uint32_t)(Hal::ins().millis() / 1000ULL));
    SaveManager::ins().saveCupGlobal(GameEngine::ins().getCupSeason(), GameEngine::ins().getLastCupTime());
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
    canvas.setTextSize(fs);

    char buf[32];
    snprintf(buf, sizeof(buf), "第 %u 届 甲虫杯", GameEngine::ins().getCupSeason());
    int tw = canvas.textWidth(buf);
    PixelRenderer::drawPixelText(cx - tw / 2, 30, buf, PixelRenderer::YELLOW, fs);

    const char* title = "甲虫杯即将开始！";
    tw = canvas.textWidth(title);
    PixelRenderer::drawPixelText(cx - tw / 2, 54, title, PixelRenderer::WHITE, fs);

    tw = canvas.textWidth("A:参赛  B:放弃");
    PixelRenderer::drawPixelText(cx - tw / 2, 100, "A:参赛  B:放弃", PixelRenderer::GRAY, fs);
}

void CupScene::drawBracket() {
    LGFX_Sprite& canvas = Hal::ins().canvas();
    float fs = PixelRenderer::getContentFontScale();
    int cx = Hal::DISPLAY_W / 2;
    canvas.setTextSize(fs);

    const char* title = "8强对阵表";
    int tw = canvas.textWidth(title);
    PixelRenderer::drawPixelText(cx - tw / 2, 8, title, PixelRenderer::YELLOW, fs);

    char buf[48];
    // 玩家固定 A1
    snprintf(buf, sizeof(buf), "你 VS %s", npcNameForIndex(sOpponents[0].index));
    PixelRenderer::drawPixelText(12, 30, buf, PixelRenderer::WHITE, fs);

    for (int i = 1; i < 4; i++) {
        snprintf(buf, sizeof(buf), "%s VS %s",
                 npcNameForIndex(sOpponents[i * 2 - 1].index),
                 npcNameForIndex(sOpponents[i * 2].index));
        PixelRenderer::drawPixelText(12, 30 + i * 14, buf, PixelRenderer::GRAY, fs);
    }

    tw = canvas.textWidth("A:开始第一轮");
    PixelRenderer::drawPixelText(cx - tw / 2, 110, "A:开始第一轮", PixelRenderer::GREEN, fs);
}

void CupScene::drawRoundIntro() {
    LGFX_Sprite& canvas = Hal::ins().canvas();
    float fs = PixelRenderer::getContentFontScale();
    int cx = Hal::DISPLAY_W / 2;
    canvas.setTextSize(fs);

    const char* roundName[3] = {"8强赛", "半决赛", "决赛"};
    char buf[32];
    snprintf(buf, sizeof(buf), "%s", roundName[sRound]);
    int tw = canvas.textWidth(buf);
    PixelRenderer::drawPixelText(cx - tw / 2, 30, buf, PixelRenderer::YELLOW, fs);

    NpcCombatant& npc = sOpponents[sRound];
    snprintf(buf, sizeof(buf), "对手: %s", npcNameForIndex(npc.index));
    tw = canvas.textWidth(buf);
    PixelRenderer::drawPixelText(cx - tw / 2, 56, buf, PixelRenderer::WHITE, fs);

    snprintf(buf, sizeof(buf), "甲虫: %s", npcBugNameForIndex(npc.index));
    tw = canvas.textWidth(buf);
    PixelRenderer::drawPixelText(cx - tw / 2, 74, buf, PixelRenderer::CYAN, fs);

    tw = canvas.textWidth("A:对战");
    PixelRenderer::drawPixelText(cx - tw / 2, 110, "A:对战", PixelRenderer::GREEN, fs);
}

void CupScene::drawResult() {
    LGFX_Sprite& canvas = Hal::ins().canvas();
    float fs = PixelRenderer::getContentFontScale();
    int cx = Hal::DISPLAY_W / 2;
    canvas.setTextSize(fs);

    const char* rt = rankText(sFinalRank);
    char buf[32];
    snprintf(buf, sizeof(buf), "%s", rt);
    int tw = canvas.textWidth(buf);
    PixelRenderer::drawPixelText(cx - tw / 2, 36, buf, PixelRenderer::YELLOW, fs);

    const char* reward = "奖励已发放";
    if (sFinalRank == 0) reward = "本次未参赛";
    tw = canvas.textWidth(reward);
    PixelRenderer::drawPixelText(cx - tw / 2, 64, reward, PixelRenderer::WHITE, fs);

    tw = canvas.textWidth("A:返回");
    PixelRenderer::drawPixelText(cx - tw / 2, 110, "A:返回", PixelRenderer::GRAY, fs);
}

const char* CupScene::rankText(uint8_t rank) const {
    switch (rank) {
        case 1: return "🏆 冠军！";
        case 2: return "🥈 亚军";
        case 4: return "🏅 四强";
        case 8: return "八强";
        default: return "未参赛";
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
