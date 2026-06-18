#include "PixelRenderer.h"
#include <cstdio>

LGFX_Sprite* PixelRenderer::canvas = nullptr;
float PixelRenderer::contentFontScale = 1.0f;

void PixelRenderer::bind(LGFX_Sprite* c) {
    canvas = c;
    if (canvas) {
        canvas->setFont(&fonts::Font0);
    }
}

void PixelRenderer::setContentFontScale(float scale) {
    contentFontScale = scale;
}

float PixelRenderer::getContentFontScale() {
    return contentFontScale;
}

void PixelRenderer::drawSprite(int x, int y,
                                const uint8_t* data, int w, int h,
                                uint16_t fgColor, uint16_t bgColor,
                                int scale) {
    if (!canvas) return;

    int bytesPerRow = (w + 7) / 8;
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            int byteIdx = row * bytesPerRow + col / 8;
            int bitIdx  = 7 - (col % 8);
            uint8_t byte = pgm_read_byte(&data[byteIdx]);
            uint16_t color = (byte >> bitIdx) & 0x01 ? fgColor : bgColor;

            if (scale == 1) {
                canvas->drawPixel(x + col, y + row, color);
            } else {
                canvas->fillRect(x + col * scale, y + row * scale,
                                 scale, scale, color);
            }
        }
    }
}

void PixelRenderer::drawPixelText(int x, int y, const char* text,
                                   uint16_t color, float scale) {
    if (!canvas) return;

    float actualScale;
    if (scale <= 0) {
        actualScale = contentFontScale;
    } else if (scale == 1) {
        actualScale = 1.0f;
    } else {
        actualScale = (float)scale;
    }
    canvas->setTextColor(color);
    canvas->setTextSize(actualScale);
    canvas->setCursor(x, y);
    canvas->print(text);
}

void PixelRenderer::drawPixelDigit(int x, int y, char digit,
                                    uint16_t color, float scale) {
    if (!canvas) return;
    char buf[2] = {digit, '\0'};
    drawPixelText(x, y, buf, color, scale);
}

void PixelRenderer::drawProgressBar(int x, int y, int w, int h,
                                     float pct, uint16_t fillColor,
                                     uint16_t bgColor) {
    if (!canvas) return;
    if (pct < 0) pct = 0;
    if (pct > 1) pct = 1;

    canvas->fillRect(x, y, w, h, bgColor);
    canvas->fillRect(x, y, (int)(w * pct), h, fillColor);
}

void PixelRenderer::fillRect(int x, int y, int w, int h, uint16_t color) {
    if (canvas) canvas->fillRect(x, y, w, h, color);
}

void PixelRenderer::drawRgb565(int x, int y, int w, int h, const uint16_t* data) {
    if (canvas && data) canvas->pushImage(x, y, w, h, data);
}

void PixelRenderer::drawRgb565Rle(int x, int y, int w, int h,
                                  const uint16_t* data, uint16_t offset,
                                  uint16_t length, bool flipX) {
    if (!canvas || !data || w <= 0 || h <= 0) return;

    const uint16_t total = (uint16_t)(w * h);
    uint16_t idx = 0;
    uint16_t pixel = 0;
    while (idx < length && pixel < total) {
        uint16_t token = pgm_read_word(&data[offset + idx++]);
        uint16_t run = token & 0x7FFF;
        if (run == 0) continue;

        if (token & 0x8000) {
            pixel += run;
            if (pixel > total) pixel = total;
            continue;
        }

        for (uint16_t i = 0; i < run && idx < length && pixel < total; ++i, ++pixel) {
            uint16_t color = pgm_read_word(&data[offset + idx++]);
            int col = pixel % w;
            int row = pixel / w;
            if (flipX) col = w - 1 - col;
            canvas->drawPixel(x + col, y + row, color);
        }
    }
}

uint16_t PixelRenderer::rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
