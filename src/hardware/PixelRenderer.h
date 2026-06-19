#pragma once
#include <M5GFX.h>
#include <cstdint>

// 像素艺术渲染器 — 基于 M5GFX/LGFX_Sprite
class PixelRenderer {
public:
    // 绑定画布
    static void bind(LGFX_Sprite* canvas);

    // ========== 像素精灵绘制 ==========
    // 绘制 1bpp 精灵（PROGMEM 数据，前景色+背景色）
    static void drawSprite(int x, int y,
                           const uint8_t* data, int w, int h,
                           uint16_t fgColor, uint16_t bgColor = 0x0000,
                           int scale = 1);

    // ========== UI 绘制 ==========
    // 使用 M5GFX 内置字体的文本
    // scale <= 0: 使用全局 contentFontScale（默认 1.0x）
    // scale == 1: 精确 1.0x
    // 其他：按传入浮点值
    static void drawPixelText(int x, int y, const char* text,
                              uint16_t color, float scale = 0);

    // 全局内容字体缩放
    static void setContentFontScale(float scale);
    static float getContentFontScale();

    // 单个像素数字
    static void drawPixelDigit(int x, int y, char digit,
                               uint16_t color, float scale = 1);

    // 进度条
    static void drawProgressBar(int x, int y, int w, int h,
                                float pct, uint16_t fillColor,
                                uint16_t bgColor = 0x0000);

    // 填充矩形
    static void fillRect(int x, int y, int w, int h, uint16_t color);

    // 绘制 RGB565 图块（可传入 PROGMEM 中的 uint16_t 数组）
    static void drawRgb565(int x, int y, int w, int h, const uint16_t* data);

    // 绘制带透明跳过段的 RGB565 RLE 精灵。token 高位为 1 表示透明 run，
    // 高位为 0 表示后续 run 个 RGB565 像素。
    static void drawRgb565Rle(int x, int y, int w, int h,
                              const uint16_t* data, uint16_t offset,
                              uint16_t length, bool flipX = false);
    static void drawRgb565RleScaled(int x, int y, int w, int h,
                                    const uint16_t* data, uint16_t offset,
                                    uint16_t length, int scale, bool flipX = false);

    // 绘制被吃掉一部分的 RLE 精灵。remaining/maxAmount 决定剩余比例，
    // 边界带固定噪声，避免像简单矩形裁切。
    static void drawRgb565RleEaten(int x, int y, int w, int h,
                                   const uint16_t* data, uint16_t offset,
                                   uint16_t length, uint8_t remaining,
                                   uint8_t maxAmount, bool flipX = false);

    // ========== 颜色工具 ==========
    static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b);

    // 常用颜色
    static constexpr uint16_t BLACK   = 0x0000;
    static constexpr uint16_t WHITE   = 0xFFFF;
    static constexpr uint16_t GREEN   = 0x07E0;
    static constexpr uint16_t BLUE    = 0x001F;
    static constexpr uint16_t RED     = 0xF800;
    static constexpr uint16_t YELLOW  = 0xFFE0;
    static constexpr uint16_t CYAN    = 0x07FF;
    static constexpr uint16_t MAGENTA = 0xF81F;
    static constexpr uint16_t GRAY    = 0x7BEF;
    static constexpr uint16_t BROWN   = 0x79E0;
    static constexpr uint16_t ORANGE  = 0xFD20;
    static constexpr uint16_t DARK_BROWN = 0x4A20;
    static constexpr uint16_t CREAM   = 0xFFDA;

private:
    static LGFX_Sprite* canvas;
    static float contentFontScale;
};
