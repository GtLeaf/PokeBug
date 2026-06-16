#include "InfoScene.h"
#include "../core/GameEngine.h"
#include "../hardware/PixelRenderer.h"
#include "../hardware/Hal.h"
#include <cstdio>
#include <cmath>

const char* InfoScene::STAGE_NAMES[4] = { "Egg", "Larva", "Pupa", "Adult" };

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
    const char* titles[PAGE_COUNT] = { "INFO", "RECORD" };
    PixelRenderer::drawPixelText(4, 4, titles[page], PixelRenderer::CYAN, 1);
    PixelRenderer::drawPixelText(140, 4, "A:Back B:Next", PixelRenderer::GRAY, 1);

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
    // 紧贴标题栏底部，避免顶部留空过多
    int y = 20 + (int)(3 * fs);
    static constexpr int STEP = 18;

    // Gen、Stage、存活状态合并到一行，节约纵向空间
    snprintf(buf, sizeof(buf), "Gen:%d %s %s",
             bug.getGeneration(),
             STAGE_NAMES[(int)bug.getStage()],
             bug.isAlive() ? "alive" : "dead");
    PixelRenderer::drawPixelText(marginX, y, buf, PixelRenderer::WHITE, fs);
    y += STEP + 2;

    // 分隔线，让上下区域不紧贴
    canvas.drawFastHLine(marginX, y - 5, Hal::DISPLAY_W - marginX * 2, PixelRenderer::GRAY);

    struct Attr { const char* name; float val; };
    Attr attrs[4] = {
        {"SIZ", bug.getSiz()},
        {"STR", bug.getStr()},
        {"END", bug.getEnd()},
        {"SPI", bug.getSpi()},
    };

    int nameX = marginX;
    int barX = nameX + (int)(35 * fs);
    int barYOffset = (int)(2 * fs);
    int barH = (int)(6 * fs);
    if (barH < 4) barH = 4;

    for (int i = 0; i < 4; i++) {
        PixelRenderer::drawPixelText(nameX, y, attrs[i].name, PixelRenderer::WHITE, fs);

        snprintf(buf, sizeof(buf), "%d", (int)roundf(attrs[i].val));
        canvas.setTextSize(fs);
        int valW = canvas.textWidth(buf);
        int valX = Hal::DISPLAY_W - marginX - valW;
        int gap = (int)(8 * fs);
        int maxBarW = valX - barX - gap;
        int barW = (int)(100 * fs);
        if (barW > maxBarW) barW = maxBarW;
        if (barW < 20) barW = 20;

        PixelRenderer::drawProgressBar(barX, y + barYOffset, barW, barH,
                                       attrs[i].val / 10.0f,
                                       PixelRenderer::GREEN, PixelRenderer::GRAY);
        PixelRenderer::drawPixelText(valX, y, buf, PixelRenderer::WHITE, fs);
        y += STEP;
    }

    snprintf(buf, sizeof(buf), "MOT:%d  HUN:%d  Sap:%d",
             bug.getMot(), bug.getHunger(), bug.getSap());
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

    PixelRenderer::drawPixelText(marginX, y, "Battle Record", PixelRenderer::YELLOW, fs);
    y += STEP;

    snprintf(buf, sizeof(buf), "Total:  %d", total);
    PixelRenderer::drawPixelText(labelX, y, buf, PixelRenderer::WHITE, fs);
    y += STEP;

    snprintf(buf, sizeof(buf), "Wins:   %d", bug.getWins());
    PixelRenderer::drawPixelText(labelX, y, buf, PixelRenderer::GREEN, fs);
    y += STEP;

    snprintf(buf, sizeof(buf), "Losses: %d", bug.getLosses());
    PixelRenderer::drawPixelText(labelX, y, buf, PixelRenderer::RED, fs);
    y += STEP;

    if (total > 0) {
        int rate = (bug.getWins() * 100) / total;
        snprintf(buf, sizeof(buf), "Rate:   %d%%", rate);
    } else {
        snprintf(buf, sizeof(buf), "Rate:   --");
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
