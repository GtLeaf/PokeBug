#include "InfoScene.h"
#include "../core/GameEngine.h"
#include "../core/UiStrings.h"
#include "../hardware/PixelRenderer.h"
#include "../hardware/Hal.h"
#include <cstdio>
#include <cmath>

const char* InfoScene::STAGE_NAMES[5] = { "Egg", "Larva", "Pupa", "Juvenile", "Adult" };

namespace {

uint16_t temperamentColor(Temperament temperament) {
    switch (temperament) {
        case Temperament::BRUTE:     return 0xF800; // 深红
        case Temperament::SWIFT:     return 0x6B7D; // 灰蓝
        case Temperament::GIANT:     return 0xFD20; // 橙褐
        case Temperament::RESILIENT: return 0xFE00; // 金色
        case Temperament::BALANCED:  return 0xFFFF; // 白/浅灰
        case Temperament::SPIRIT:    return 0x07E0; // 青绿
    }
    return PixelRenderer::WHITE;
}

uint16_t potentialColor(uint8_t cap) {
    if (cap <= 10) return PixelRenderer::rgb565(150, 150, 160); // Low
    if (cap <= 13) return PixelRenderer::WHITE;                // Common
    if (cap <= 16) return PixelRenderer::rgb565(80, 150, 255); // Rare
    if (cap <= 19) return PixelRenderer::rgb565(163, 53, 238); // Epic
    return PixelRenderer::ORANGE;                              // Legendary
}

}

void InfoScene::onEnter() {
    page = 0;
}

void InfoScene::onExit() {}

SceneID InfoScene::update() {
    return nextScene;
}

void InfoScene::render() {
    LGFX_Sprite& canvas = Hal::ins().canvas();

    canvas.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::BLACK);

    // 标题栏
    canvas.fillRect(0, 0, Hal::DISPLAY_W, 20, PixelRenderer::rgb565(25, 25, 40));
    float fs = PixelRenderer::getContentFontScale();
    const char* titles[PAGE_COUNT] = {
        UiStrings::INFO_TITLE,
        UiStrings::ATTR_TITLE,
        UiStrings::RECORD_TITLE,
    };
    PixelRenderer::drawPixelText(4, 4, titles[page], PixelRenderer::CYAN, fs);
    canvas.setTextSize(fs);
    const char* nav = UiStrings::INFO_NAV;
    int titleW = canvas.textWidth(titles[page]);
    int navW = canvas.textWidth(nav);
    if (titleW + navW + 14 > Hal::DISPLAY_W) {
        nav = UiStrings::INFO_NAV_SHORT;
        navW = canvas.textWidth(nav);
    }
    PixelRenderer::drawPixelText(Hal::DISPLAY_W - navW - 4, 4, nav, PixelRenderer::GRAY, fs);

    renderPageIndicator();

    switch (page) {
        case 0: renderStatus(); break;
        case 1: renderAttributes(); break;
        case 2: renderRecord(); break;
    }
}

void InfoScene::renderPageIndicator() {
    LGFX_Sprite& canvas = Hal::ins().canvas();
    static constexpr int DOT_W = 4;
    static constexpr int DOT_GAP = 4;
    static constexpr int DOT_H = 6;
    static constexpr int MARGIN_RIGHT = 8;
    int totalH = PAGE_COUNT * DOT_H + (PAGE_COUNT - 1) * DOT_GAP;
    int startY = (Hal::DISPLAY_H - totalH) / 2;
    int x = Hal::DISPLAY_W - DOT_W - MARGIN_RIGHT;

    for (int i = 0; i < PAGE_COUNT; i++) {
        int y = startY + i * (DOT_H + DOT_GAP);
        uint16_t color = (i == page) ? PixelRenderer::CYAN : PixelRenderer::GRAY;
        canvas.fillRect(x, y, DOT_W, DOT_H, color);
    }
}

void InfoScene::renderStatus() {
    LGFX_Sprite& canvas = Hal::ins().canvas();
    Bug& bug = GameEngine::ins().getBug();
    char buf[48];

    float fs = PixelRenderer::getContentFontScale();
    int marginX = (int)(10 * fs);
    // 顶部留白，避免贴标题栏太近
    int y = 20 + (int)(3 * fs);
    int rowStep = (int)(12 * fs);
    if (rowStep < 18) rowStep = 18;
    if (rowStep > 21) rowStep = 21;

    snprintf(buf, sizeof(buf), "%s: %d", UiStrings::GEN, bug.getGeneration());
    PixelRenderer::drawPixelText(marginX, y, buf, PixelRenderer::WHITE, fs);
    y += rowStep;

    snprintf(buf, sizeof(buf), "Stage: %s", STAGE_NAMES[(int)bug.getStage()]);
    PixelRenderer::drawPixelText(marginX, y, buf, PixelRenderer::WHITE, fs);
    y += rowStep;

    snprintf(buf, sizeof(buf), "%s: ", UiStrings::TYPE);
    PixelRenderer::drawPixelText(marginX, y, buf, PixelRenderer::WHITE, fs);
    canvas.setTextSize(fs);
    int typeLabelW = canvas.textWidth(buf);
    PixelRenderer::drawPixelText(marginX + typeLabelW, y, bug.getTemperamentName(),
                                 temperamentColor(bug.getTemperament()), fs);
    y += rowStep;

    canvas.drawFastHLine(marginX, y - (int)(4 * fs), Hal::DISPLAY_W - marginX * 2 - 12, PixelRenderer::GRAY);
    y += (int)(4 * fs);

    snprintf(buf, sizeof(buf), "%s: %d", UiStrings::MOT, bug.getMot());
    PixelRenderer::drawPixelText(marginX, y, buf, PixelRenderer::WHITE, fs);
    y += rowStep;

    snprintf(buf, sizeof(buf), "%s: %d  %s: %d",
             UiStrings::HUN, bug.getHunger(),
             UiStrings::DROP, bug.getFoodCount(FoodType::DROP));
    PixelRenderer::drawPixelText(marginX, y, buf, PixelRenderer::WHITE, fs);
}

void InfoScene::renderAttributes() {
    LGFX_Sprite& canvas = Hal::ins().canvas();
    Bug& bug = GameEngine::ins().getBug();
    char buf[16];

    float fs = PixelRenderer::getContentFontScale();
    int marginX = (int)(10 * fs);
    int y = 20 + (int)(5 * fs);
    int rowStep = (int)(12 * fs);
    if (rowStep < 18) rowStep = 18;
    if (rowStep > 21) rowStep = 21;
    int barH = (int)(4 * fs);
    if (barH < 3) barH = 3;

    struct Attr { const char* name; float val; uint8_t cap; };
    Attr attrs[5] = {
        {"SIZ", bug.getSiz(), bug.getSizCap()},
        {"STR", bug.getStr(), bug.getStrCap()},
        {"END", bug.getEnd(), bug.getEndCap()},
        {"SPD", bug.getSpd(), bug.getSpdCap()},
        {"SPI", bug.getSpi(), bug.getSpiCap()},
    };

    canvas.setTextSize(fs);
    int valueRight = Hal::DISPLAY_W - (int)(20 * fs);
    int valueColW = canvas.textWidth("99");
    int barX = marginX + (int)(32 * fs);
    int barRight = valueRight - valueColW - (int)(8 * fs);
    int barW = barRight - barX;
    if (barW < 40) barW = 40;

    for (int i = 0; i < 5; i++) {
        uint16_t color = potentialColor(attrs[i].cap);
        PixelRenderer::drawPixelText(marginX, y, attrs[i].name, color, fs);

        float ratio = attrs[i].cap == 0 ? 0.0f : attrs[i].val / attrs[i].cap;
        if (ratio > 1.0f) ratio = 1.0f;
        PixelRenderer::drawProgressBar(barX, y + (rowStep - barH) / 2, barW, barH,
                                       ratio,
                                       PixelRenderer::GREEN, PixelRenderer::GRAY);

        snprintf(buf, sizeof(buf), "%d", (int)roundf(attrs[i].val));
        int valW = canvas.textWidth(buf);
        PixelRenderer::drawPixelText(valueRight - valW, y, buf, color, fs);
        y += rowStep;
    }
}

void InfoScene::renderRecord() {
    Bug& bug = GameEngine::ins().getBug();
    char buf[48];

    float fs = PixelRenderer::getContentFontScale();
    int labelX = (int)(48 * fs);
    // 紧贴标题栏底部，避免顶部留空过多
    int y = 20 + (int)(8 * fs);
    int step = (int)(12 * fs);
    if (step < 18) step = 18;
    if (step > 22) step = 22;

    int total = (int)bug.getWins() + (int)bug.getLosses();

    snprintf(buf, sizeof(buf), "%s:  %d", UiStrings::TOTAL, total);
    PixelRenderer::drawPixelText(labelX, y, buf, PixelRenderer::WHITE, fs);
    y += step;

    snprintf(buf, sizeof(buf), "%s:   %d", UiStrings::WINS, bug.getWins());
    PixelRenderer::drawPixelText(labelX, y, buf, PixelRenderer::GREEN, fs);
    y += step;

    snprintf(buf, sizeof(buf), "%s: %d", UiStrings::LOSSES, bug.getLosses());
    PixelRenderer::drawPixelText(labelX, y, buf, PixelRenderer::RED, fs);
    y += step;

    if (total > 0) {
        int rate = (bug.getWins() * 100) / total;
        snprintf(buf, sizeof(buf), "%s:   %d%%", UiStrings::RATE, rate);
    } else {
        snprintf(buf, sizeof(buf), "%s:   --", UiStrings::RATE);
    }
    PixelRenderer::drawPixelText(labelX, y, buf, PixelRenderer::CYAN, fs);
}

bool InfoScene::onButton(const ButtonEvent& ev) {
    if (ev.action == BtnAction::LONG_PRESS) {
        if (ev.btn == 0) {
            // 长按 A：任何地方都返回主界面（培养缸）
            nextScene = SCENE_TERRARIUM;
            return true;
        }
        if (ev.btn == 1) {
            // 长按 B：返回上级菜单
            nextScene = SCENE_MENU;
            return true;
        }
    }

    if (ev.action == BtnAction::PRESSED) {
        if (ev.btn == 0) {
            // A：返回上级菜单
            nextScene = SCENE_MENU;
            return true;
        }
        if (ev.btn == 1) {
            // B：切换下一页
            page++;
            if (page >= PAGE_COUNT) page = 0;
            return true;
        }
    }
    return false;
}
