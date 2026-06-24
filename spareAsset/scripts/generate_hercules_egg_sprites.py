#!/usr/bin/env python3
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
SRC_DIR = ROOT / "spareAsset/origin/beetle/hercules/egg"
OUT_H = ROOT / "src/assets/HerculesEggSprites.h"
OUT_CPP = ROOT / "src/assets/HerculesEggSprites.cpp"
SPARE = ROOT / "spareAsset"
EXTRACTED = SPARE / "extracted/beetle/hercules/egg"
PREVIEWS = SPARE / "previews/beetle/hercules/egg"
GENERATED = SPARE / "generated/src_assets"

# Usage:
# 1. Put the egg sheet under spareAsset/origin/beetle/hercules/egg/.
# 2. The sheet must contain exactly 3 frames from left to right:
#    early, middle, late egg chamber.
# 3. The generated frame contains the substrate nest plus eggs.
#    Current target is 58x36 px: same width as the pupa chamber, about half
#    the pupa height, so eggs read as an earlier and lower life stage.
# 4. Run:
#       python3 spareAsset/scripts/generate_hercules_egg_sprites.py
#    Then build with:
#       pio run

FRAME_COUNT = 3
EGG_FRAME_W = 58
EGG_FRAME_H = 36
EGG_CHAMBER_MAX_W = 58
EGG_CHAMBER_MAX_H = 36


def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def is_green_bg(r, g, b, a):
    if a < 80:
        return True
    return g > 80 and r < 180 and b < 180 and g > r * 1.18 and g > b * 1.18


def remove_green_bg(frame):
    im = frame.convert("RGBA")
    pix = im.load()
    for y in range(im.height):
        for x in range(im.width):
            r, g, b, a = pix[x, y]
            if is_green_bg(r, g, b, a):
                pix[x, y] = (0, 0, 0, 0)
    return im


def trim_alpha(im):
    bbox = im.getbbox()
    return im.crop(bbox) if bbox else Image.new("RGBA", (1, 1), (0, 0, 0, 0))


def source_sheet():
    candidates = sorted(SRC_DIR.glob("*.png"))
    if not candidates:
        raise FileNotFoundError(f"No egg PNG found in {SRC_DIR}")
    return candidates[0]


def make_frames():
    EXTRACTED.mkdir(parents=True, exist_ok=True)
    PREVIEWS.mkdir(parents=True, exist_ok=True)
    GENERATED.mkdir(parents=True, exist_ok=True)

    path = source_sheet()
    sheet = Image.open(path).convert("RGBA")
    if sheet.width % FRAME_COUNT != 0:
        raise ValueError(f"Egg sheet width {sheet.width} is not divisible by {FRAME_COUNT}")

    source_w = sheet.width // FRAME_COUNT
    frames = []
    for i in range(FRAME_COUNT):
        crop = sheet.crop((i * source_w, 0, (i + 1) * source_w, sheet.height))
        crop = trim_alpha(remove_green_bg(crop))
        scale = min(EGG_CHAMBER_MAX_W / crop.width, EGG_CHAMBER_MAX_H / crop.height)
        resized = crop.resize(
            (max(1, round(crop.width * scale)), max(1, round(crop.height * scale))),
            Image.Resampling.LANCZOS,
        )
        resized = trim_alpha(remove_green_bg(resized))
        frame = Image.new("RGBA", (EGG_FRAME_W, EGG_FRAME_H), (0, 0, 0, 0))
        frame.alpha_composite(resized, ((EGG_FRAME_W - resized.width) // 2, EGG_FRAME_H - resized.height))
        frames.append(frame)
        frame.save(EXTRACTED / f"egg_{i}.png")
        print(
            f"egg_{i}: source {source_w}x{sheet.height}, bbox {crop.width}x{crop.height} "
            f"-> {resized.width}x{resized.height} in {EGG_FRAME_W}x{EGG_FRAME_H}"
        )
    return frames


def image_data(img):
    getter = getattr(img, "get_flattened_data", None)
    return getter() if getter else img.getdata()


def encode(img):
    pixels = list(image_data(img))
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
    sheet = Image.new("RGBA", (EGG_FRAME_W * len(frames), EGG_FRAME_H), (0, 0, 0, 0))
    for i, frame in enumerate(frames):
        sheet.alpha_composite(frame, (i * EGG_FRAME_W, 0))
    sheet.save(PREVIEWS / "egg_sheet_preview.png")


def write_outputs(frames):
    h = f"""#pragma once
#include <Arduino.h>
#include <cstdint>

namespace HerculesEggSprites {{

static constexpr uint8_t FRAME_COUNT = {FRAME_COUNT};
static constexpr uint8_t FRAME_W = {EGG_FRAME_W};
static constexpr uint8_t FRAME_H = {EGG_FRAME_H};

struct RleFrame {{
    uint16_t offset;
    uint16_t length;
    uint8_t width;
    uint8_t height;
}};

extern const RleFrame FRAMES[] PROGMEM;
extern const uint16_t RLE[] PROGMEM;

}}
"""
    OUT_H.write_text(h, encoding="utf-8")

    chunks = []
    offset = 0
    meta = []
    for frame in frames:
        encoded = encode(frame)
        meta.append((offset, len(encoded), EGG_FRAME_W, EGG_FRAME_H))
        chunks.extend(encoded)
        offset += len(encoded)

    cpp = ['#include "HerculesEggSprites.h"', "", "namespace HerculesEggSprites {", ""]
    cpp.append("const RleFrame FRAMES[] PROGMEM = {")
    for off, length, w, hgt in meta:
        cpp.append(f"    {{ {off}, {length}, {w}, {hgt} }},")
    cpp.append("};")
    cpp.append("")
    cpp.append("const uint16_t RLE[] PROGMEM = {")
    cpp.append(fmt(chunks))
    cpp.append("};")
    cpp.append("")
    cpp.append("}")
    cpp.append("")
    OUT_CPP.write_text("\n".join(cpp), encoding="utf-8")

    (GENERATED / OUT_H.name).write_text(OUT_H.read_text(encoding="utf-8"), encoding="utf-8")
    (GENERATED / OUT_CPP.name).write_text(OUT_CPP.read_text(encoding="utf-8"), encoding="utf-8")


def main():
    frames = make_frames()
    write_preview(frames)
    write_outputs(frames)


if __name__ == "__main__":
    main()
