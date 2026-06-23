#!/usr/bin/env python3
from collections import deque
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "asset/food/food.png"
OUT_H = ROOT / "src/assets/FoodAssets.h"
OUT_CPP = ROOT / "src/assets/FoodAssets.cpp"
SPARE = ROOT / "spareAsset"
EXTRACTED = SPARE / "extracted/food"
PREVIEWS = SPARE / "previews/food"
GENERATED = SPARE / "generated/src_assets"

# Usage:
# 1. Put food source art in asset/food/food.png.
# 2. Keep each food style as a separate connected object on transparent or green background.
# 3. Adjust FOOD_FRAME_W/FOOD_FRAME_H below when the in-game food size changes.
# 4. Run: python3 spareAsset/scripts/generate_food_assets.py
#
# The script writes src/assets/FoodAssets.*, extracted frames, a preview sheet,
# and a backup copy under spareAsset/generated/src_assets/.

# Size policy. Change these two values to resize every food style together.
FOOD_FRAME_W = 35
FOOD_FRAME_H = 17


def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def is_bg(r, g, b, a, bg):
    if a < 80:
        return True
    green = g > 145 and r < 155 and b < 155 and g > r * 1.35 and g > b * 1.35
    if green:
        return True
    return abs(r - bg[0]) < 8 and abs(g - bg[1]) < 8 and abs(b - bg[2]) < 8


def transparentized():
    im = Image.open(SRC).convert("RGBA")
    bg = im.getpixel((0, 0))[:3]
    pix = im.load()
    w, h = im.size
    mask = [[False] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            r, g, b, a = pix[x, y]
            if is_bg(r, g, b, a, bg):
                pix[x, y] = (0, 0, 0, 0)
            else:
                mask[y][x] = True
    return im, mask


def components():
    im, mask = transparentized()
    w, h = im.size
    seen = [[False] * w for _ in range(h)]
    comps = []
    for yy in range(h):
        for xx in range(w):
            if not mask[yy][xx] or seen[yy][xx]:
                continue
            q = deque([(xx, yy)])
            seen[yy][xx] = True
            minx = maxx = xx
            miny = maxy = yy
            count = 0
            while q:
                x, y = q.popleft()
                count += 1
                minx = min(minx, x)
                maxx = max(maxx, x)
                miny = min(miny, y)
                maxy = max(maxy, y)
                for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                    if 0 <= nx < w and 0 <= ny < h and mask[ny][nx] and not seen[ny][nx]:
                        seen[ny][nx] = True
                        q.append((nx, ny))
            if count > 50:
                comps.append((minx, miny, maxx + 1, maxy + 1))
    comps.sort(key=lambda b: (b[0], b[1]))
    return im, comps


def trim_alpha(im):
    bbox = im.getbbox()
    return im.crop(bbox) if bbox else Image.new("RGBA", (1, 1), (0, 0, 0, 0))


def make_frames():
    im, comps = components()
    frames = []
    for i, (x1, y1, x2, y2) in enumerate(comps):
        crop = trim_alpha(im.crop((x1, y1, x2, y2)))
        scale = min(FOOD_FRAME_W / crop.width, FOOD_FRAME_H / crop.height)
        resized = crop.resize(
            (max(1, round(crop.width * scale)), max(1, round(crop.height * scale))),
            Image.Resampling.LANCZOS,
        )
        resized = trim_alpha(resized)
        canvas = Image.new("RGBA", (FOOD_FRAME_W, FOOD_FRAME_H), (0, 0, 0, 0))
        canvas.alpha_composite(resized, ((FOOD_FRAME_W - resized.width) // 2, FOOD_FRAME_H - resized.height))
        frames.append(canvas)
        canvas.save(EXTRACTED / f"food_{i}.png")
        print(f"food_{i}: {crop.width}x{crop.height} -> {resized.width}x{resized.height} in {FOOD_FRAME_W}x{FOOD_FRAME_H}")
    return frames


def encode(img):
    pixels = list(img.get_flattened_data() if hasattr(img, "get_flattened_data") else img.getdata())
    out = []
    i = 0
    while i < len(pixels):
        r, g, b, a = pixels[i]
        if a < 80:
            j = i + 1
            while j < len(pixels) and pixels[j][3] < 80 and (j - i) < 0x7FFF:
                j += 1
            out.append(0x8000 | (j - i))
            i = j
        else:
            vals = []
            while i < len(pixels) and len(vals) < 0x7FFF:
                r, g, b, a = pixels[i]
                if a < 80:
                    break
                vals.append(rgb565(r, g, b))
                i += 1
            out.append(len(vals))
            out.extend(vals)
    return out


def fmt(vals):
    return "\n".join(
        "    " + ", ".join(f"0x{v:04X}" for v in vals[i : i + 12]) + ","
        for i in range(0, len(vals), 12)
    )


def write_preview(frames):
    sheet = Image.new("RGBA", (FOOD_FRAME_W * len(frames), FOOD_FRAME_H), (0, 0, 0, 0))
    for i, frame in enumerate(frames):
        sheet.alpha_composite(frame, (i * FOOD_FRAME_W, 0))
    sheet.save(PREVIEWS / "food_sheet_preview.png")


def write_outputs(frames):
    OUT_H.write_text(
        f"""#pragma once
#include <Arduino.h>
#include <cstdint>

namespace FoodAssets {{

static constexpr uint8_t FRAME_W = {FOOD_FRAME_W};
static constexpr uint8_t FRAME_H = {FOOD_FRAME_H};

struct RleFrame {{
    uint16_t offset;
    uint16_t length;
}};

extern const uint8_t SPRITE_COUNT;
extern const RleFrame SPRITE_FRAMES[] PROGMEM;
extern const uint16_t SPRITE_RLE[] PROGMEM;

}}
""",
        encoding="utf-8",
    )
    data = []
    metas = []
    for frame in frames:
        enc = encode(frame)
        metas.append((len(data), len(enc)))
        data.extend(enc)
    cpp = ['#include "FoodAssets.h"', "", "namespace FoodAssets {", ""]
    cpp.append(f"const uint8_t SPRITE_COUNT = {len(frames)};")
    cpp.append("const RleFrame SPRITE_FRAMES[] PROGMEM = {")
    for off, length in metas:
        cpp.append(f"    {{ {off}, {length} }},")
    cpp.append("};")
    cpp.append("")
    cpp.append("const uint16_t SPRITE_RLE[] PROGMEM = {")
    cpp.append(fmt(data))
    cpp.append("};")
    cpp.append("")
    cpp.append("}")
    cpp.append("")
    OUT_CPP.write_text("\n".join(cpp), encoding="utf-8")


def main():
    EXTRACTED.mkdir(parents=True, exist_ok=True)
    PREVIEWS.mkdir(parents=True, exist_ok=True)
    GENERATED.mkdir(parents=True, exist_ok=True)
    frames = make_frames()
    write_preview(frames)
    write_outputs(frames)
    (GENERATED / OUT_H.name).write_text(OUT_H.read_text(encoding="utf-8"), encoding="utf-8")
    (GENERATED / OUT_CPP.name).write_text(OUT_CPP.read_text(encoding="utf-8"), encoding="utf-8")


if __name__ == "__main__":
    main()
