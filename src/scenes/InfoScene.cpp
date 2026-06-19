#include "InfoScene.h"
#include "../core/GameEngine.h"
#include "../core/UiStrings.h"
#include "../hardware/PixelRenderer.h"
#include "../hardware/Hal.h"
#include <cstdio>
#include <cmath>

const char* InfoScene::STAGE_NAMES[5] = { "Egg", "Larva", "Pupa", "Juvenile", "Adult" };

void InfoScene::onEnter() {
    page = 0;
}

void InfoScene::onExit() {}

SceneID InfoScene::update() {
    return nextScene;
}

void InfoScene::render() {
    LGFX_Sprite& canvas = Hal::ins().canvas();
    Bug& bug = GameEngine::ins().getBug();

    canvas.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::BLACK);

    // 标题栏
    canvas.fillRect(0, 0, Hal::DISPLAY_W, 20, PixelRenderer::rgb565(25, 25, 40));
    float fs = PixelRenderer::getContentFontScale();
    const char* titles[PAGE_COUNT] = { UiStrings::INFO_TITLE, UiStrings::RECORD_TITLE };
    PixelRenderer::drawPixelText(4, 4, titles[page], PixelRenderer::CYAN, fs);
    PixelRenderer::drawPixelText(140, 4, UiStrings::INFO_NAV, PixelRenderer::GRAY, fs);

    renderPageIndicator();

    switch (page) {
        case 0: renderStatus(); break;
        case 1: renderRecord(); break;
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
    static constexpr int STEP = 24;

    // Gen、Stage、气质合并到一行，节约纵向空间
    snprintf(buf, sizeof(buf), "%s:%d %s %s",
             UiStrings::GEN,
             bug.getGeneration(),
             STAGE_NAMES[(int)bug.getStage()],
             bug.getTemperamentName());
    PixelRenderer::drawPixelText(marginX, y, buf, PixelRenderer::WHITE, fs);
    y += STEP;

    // 分隔线，让上下区域不紧贴
    canvas.drawFastHLine(marginX, y - 6, Hal::DISPLAY_W - marginX * 2, PixelRenderer::GRAY);
    y += 4;

    struct Attr { const char* name; float val; };
    Attr attrs[5] = {
        {"SIZ", bug.getSiz()},
        {"STR", bug.getStr()},
        {"END", bug.getEnd()},
        {"SPD", bug.getSpd()},
        {"SPI", bug.getSpi()},
    };

    static constexpr int ATTR_STEP = 16;
    int colW = (Hal::DISPLAY_W - marginX * 2) / 2;
    int barH = (int)(4 * fs);
    if (barH < 3) barH = 3;
    int barYOffset = (ATTR_STEP - barH) / 2;

    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 2; col++) {
            int idx = row * 2 + col;
            if (idx >= 5) break;
            int x = marginX + col * colW;

            // 属性名
            PixelRenderer::drawPixelText(x, y, attrs[idx].name, PixelRenderer::WHITE, fs);

            // 数值靠右
            snprintf(buf, sizeof(buf), "%d", (int)roundf(attrs[idx].val));
            canvas.setTextSize(fs);
            int valW = canvas.textWidth(buf);
            int valX = x + colW - valW - (int)(4 * fs);
            PixelRenderer::drawPixelText(valX, y, buf, PixelRenderer::WHITE, fs);

            // 进度条：在属性名和数值之间
            int nameW = canvas.textWidth(attrs[idx].name);
            int barX = x + nameW + (int)(6 * fs);
            int barW = valX - barX - (int)(4 * fs);
            if (barW < 10) barW = 10;
            PixelRenderer::drawProgressBar(barX, y + barYOffset, barW, barH,
                                           attrs[idx].val / 10.0f,
                                           PixelRenderer::GREEN, PixelRenderer::GRAY);
        }
        y += ATTR_STEP;
    }

    // MOT / HUN / Drop 合并到底部一行小字
    y += 6;
    snprintf(buf, sizeof(buf), "%s:%d %s:%d",
             UiStrings::MOT, bug.getMot(),
             UiStrings::HUN, bug.getHunger());
    PixelRenderer::drawPixelText(marginX, y, buf, PixelRenderer::WHITE, fs);
}

void InfoScene::renderRecord() {
    Bug& bug = GameEngine::ins().getBug();
    char buf[48];

    float fs = PixelRenderer::getContentFontScale();
    int marginX = (int)(10 * fs);
    int labelX = (int)(55 * fs);
    // 紧贴标题栏底部，避免顶部留空过多
    int y = 20 + (int)(3 * fs);
    static constexpr int STEP = 18;

    int total = (int)bug.getWins() + (int)bug.getLosses();

    PixelRenderer::drawPixelText(marginX, y, UiStrings::RECORD_TITLE, PixelRenderer::YELLOW, fs);
    y += STEP;

    snprintf(buf, sizeof(buf), "%s:  %d", UiStrings::TOTAL, total);
    PixelRenderer::drawPixelText(labelX, y, buf, PixelRenderer::WHITE, fs);
    y += STEP;

    snprintf(buf, sizeof(buf), "%s:   %d", UiStrings::WINS, bug.getWins());
    PixelRenderer::drawPixelText(labelX, y, buf, PixelRenderer::GREEN, fs);
    y += STEP;

    snprintf(buf, sizeof(buf), "%s: %d", UiStrings::LOSSES, bug.getLosses());
    PixelRenderer::drawPixelText(labelX, y, buf, PixelRenderer::RED, fs);
    y += STEP;

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
