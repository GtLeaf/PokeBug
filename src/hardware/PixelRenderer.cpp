#include "PixelRenderer.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

LGFX_Sprite* PixelRenderer::canvas = nullptr;
float PixelRenderer::contentFontScale = 1.0f;

void PixelRenderer::bind(LGFX_Sprite* c) {
    canvas = c;
    if (canvas) {
        applyTextStyle(contentFontScale);
    }
}

void PixelRenderer::setContentFontScale(float scale) {
    contentFontScale = scale;
}

float PixelRenderer::getContentFontScale() {
    return contentFontScale;
}

void PixelRenderer::applyTextStyle(float scale) {
    if (!canvas) return;

    float actualScale = scale <= 0.0f ? contentFontScale : scale;
    if (actualScale < 1.65f) {
        canvas->setFont(&fonts::DejaVu12);
    } else {
        canvas->setFont(&fonts::DejaVu18);
    }
    canvas->setTextSize(1);
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

    applyTextStyle(scale);
    canvas->setTextColor(color);
    canvas->setTextWrap(false);
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

void PixelRenderer::drawIndexed8(int x, int y, int w, int h,
                                 const uint8_t* indices,
                                 const uint16_t* palette) {
    if (!canvas || !indices || !palette || w <= 0 || h <= 0) return;
    if (w > 240) return;

    uint16_t line[240];
    for (int row = 0; row < h; ++row) {
        int base = row * w;
        for (int col = 0; col < w; ++col) {
            uint8_t idx = pgm_read_byte(&indices[base + col]);
            line[col] = pgm_read_word(&palette[idx]);
        }
        canvas->pushImage(x, y + row, w, 1, line);
    }
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

void PixelRenderer::drawRgb565RleMapped(int x, int y, int w, int h,
                                        const uint16_t* data, uint16_t offset,
                                        uint16_t length,
                                        uint16_t keyMain, uint16_t targetMain,
                                        uint16_t keyShadow, uint16_t targetShadow,
                                        uint16_t keyMarking, uint16_t targetMarking,
                                        bool flipX) {
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
            if (color == keyMain) color = targetMain;
            else if (color == keyShadow) color = targetShadow;
            else if (color == keyMarking) color = targetMarking;

            int col = pixel % w;
            int row = pixel / w;
            if (flipX) col = w - 1 - col;
            canvas->drawPixel(x + col, y + row, color);
        }
    }
}

void PixelRenderer::drawRgb565RleMappedScaled(int x, int y, int w, int h,
                                              const uint16_t* data, uint16_t offset,
                                              uint16_t length, float scale,
                                              uint16_t keyMain, uint16_t targetMain,
                                              uint16_t keyShadow, uint16_t targetShadow,
                                              uint16_t keyMarking, uint16_t targetMarking,
                                              bool flipX) {
    if (!canvas || !data || w <= 0 || h <= 0 || scale <= 0.0f) return;
    if (scale == 1.0f) {
        drawRgb565RleMapped(x, y, w, h, data, offset, length,
                            keyMain, targetMain,
                            keyShadow, targetShadow,
                            keyMarking, targetMarking,
                            flipX);
        return;
    }

    auto mapColor = [&](uint16_t color) -> uint16_t {
        if (color == keyMain) return targetMain;
        if (color == keyShadow) return targetShadow;
        if (color == keyMarking) return targetMarking;
        return color;
    };

    if (scale > 1.0f) {
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
                uint16_t color = mapColor(pgm_read_word(&data[offset + idx++]));
                int col = pixel % w;
                int row = pixel / w;
                if (flipX) col = w - 1 - col;
                int drawX = (int)(x + col * scale);
                int drawY = (int)(y + row * scale);
                int drawW = (int)ceilf(scale);
                int drawH = (int)ceilf(scale);
                canvas->fillRect(drawX, drawY, drawW, drawH, color);
            }
        }
        return;
    }

    int outW = (int)(w * scale);
    int outH = (int)(h * scale);
    if (outW <= 0) outW = 1;
    if (outH <= 0) outH = 1;

    size_t pixelCount = (size_t)w * h;
    uint16_t* buf = (uint16_t*)malloc(pixelCount * sizeof(uint16_t));
    uint8_t* opaque = (uint8_t*)calloc(pixelCount, sizeof(uint8_t));
    if (!buf || !opaque) {
        if (buf) free(buf);
        if (opaque) free(opaque);
        drawRgb565RleMapped(x, y, w, h, data, offset, length,
                            keyMain, targetMain,
                            keyShadow, targetShadow,
                            keyMarking, targetMarking,
                            flipX);
        return;
    }

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
            buf[pixel] = mapColor(pgm_read_word(&data[offset + idx++]));
            opaque[pixel] = 1;
        }
    }

    for (int row = 0; row < outH; row++) {
        int srcRow = (int)(row / scale);
        if (srcRow >= h) srcRow = h - 1;
        for (int col = 0; col < outW; col++) {
            int srcCol = (int)(col / scale);
            if (srcCol >= w) srcCol = w - 1;
            int finalCol = flipX ? (w - 1 - srcCol) : srcCol;
            size_t srcIndex = (size_t)srcRow * w + finalCol;
            if (opaque[srcIndex]) {
                canvas->drawPixel(x + col, y + row, buf[srcIndex]);
            }
        }
    }

    free(opaque);
    free(buf);
}

void PixelRenderer::drawRgb565RleScaled(int x, int y, int w, int h,
                                        const uint16_t* data, uint16_t offset,
                                        uint16_t length, float scale, bool flipX) {
    if (!canvas || !data || w <= 0 || h <= 0 || scale <= 0.0f) return;
    if (scale == 1.0f) {
        drawRgb565Rle(x, y, w, h, data, offset, length, flipX);
        return;
    }

    // scale > 1：每个源像素扩展为 scale x scale 的色块
    if (scale > 1.0f) {
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
                int drawX = (int)(x + col * scale);
                int drawY = (int)(y + row * scale);
                int drawW = (int)ceilf(scale);
                int drawH = (int)ceilf(scale);
                canvas->fillRect(drawX, drawY, drawW, drawH, color);
            }
        }
        return;
    }

    // scale < 1：先解压 RLE，再最近邻下采样绘制
    int outW = (int)(w * scale);
    int outH = (int)(h * scale);
    if (outW <= 0) outW = 1;
    if (outH <= 0) outH = 1;

    size_t pixelCount = (size_t)w * h;
    uint16_t* buf = (uint16_t*)malloc(pixelCount * sizeof(uint16_t));
    uint8_t* opaque = (uint8_t*)calloc(pixelCount, sizeof(uint8_t));
    if (!buf || !opaque) {
        if (buf) free(buf);
        if (opaque) free(opaque);
        drawRgb565Rle(x, y, w, h, data, offset, length, flipX);
        return;
    }

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
            buf[pixel] = pgm_read_word(&data[offset + idx++]);
            opaque[pixel] = 1;
        }
    }

    for (int row = 0; row < outH; row++) {
        int srcRow = (int)(row / scale);
        if (srcRow >= h) srcRow = h - 1;
        for (int col = 0; col < outW; col++) {
            int srcCol = (int)(col / scale);
            if (srcCol >= w) srcCol = w - 1;
            int finalCol = flipX ? (w - 1 - srcCol) : srcCol;
            size_t srcIndex = (size_t)srcRow * w + finalCol;
            if (opaque[srcIndex]) {
                canvas->drawPixel(x + col, y + row, buf[srcIndex]);
            }
        }
    }

    free(opaque);
    free(buf);
}

void PixelRenderer::drawRgb565RleEaten(int x, int y, int w, int h,
                                       const uint16_t* data, uint16_t offset,
                                       uint16_t length, uint8_t remaining,
                                       uint8_t maxAmount, bool flipX) {
    if (!canvas || !data || w <= 0 || h <= 0 || maxAmount == 0 || remaining == 0) return;
    if (remaining > maxAmount) remaining = maxAmount;

    const uint16_t total = (uint16_t)(w * h);
    int keepBase = (w * remaining) / maxAmount;
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
            int srcCol = pixel % w;
            int row = pixel / w;

            uint8_t edgeNoise = (uint8_t)((srcCol * 13 + row * 7 + row * row * 3) & 0x07);
            int biteEdge = keepBase - 3 + edgeNoise;
            bool kept = srcCol < biteEdge;
            if (remaining == maxAmount) kept = true;

            if (kept) {
                int col = flipX ? (w - 1 - srcCol) : srcCol;
                canvas->drawPixel(x + col, y + row, color);
            }
        }
    }
}

uint16_t PixelRenderer::rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
