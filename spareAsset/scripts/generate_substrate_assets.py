#!/usr/bin/env python3
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "spareAsset/origin/goods/food/substrate/substrate.png"
OUT_H = ROOT / "src/assets/SubstrateAssets.h"
OUT_CPP = ROOT / "src/assets/SubstrateAssets.cpp"
SPARE = ROOT / "spareAsset"
EXTRACTED = SPARE / "extracted/goods/food/substrate"
PREVIEWS = SPARE / "previews/goods/food/substrate"
GENERATED = SPARE / "generated/src_assets"

TARGET_HEIGHTS = (22, 34, 48)


def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def is_bg(r, g, b, a, bg):
    if a < 80:
        return True
    green = (
        (g > 145 and r < 170 and b < 170 and g > r * 1.18 and g > b * 1.18)
        or (g > 55 and g > r + 22 and g > b + 22)
    )
    if green:
        return True
    return abs(r - bg[0]) < 8 and abs(g - bg[1]) < 8 and abs(b - bg[2]) < 8


def remove_green_fringe(im):
    pix = im.load()
    for y in range(im.height):
        for x in range(im.width):
            r, g, b, a = pix[x, y]
            if is_bg(r, g, b, a, (0, 255, 0)):
                pix[x, y] = (0, 0, 0, 0)
    return im


def transparentized():
    im = Image.open(SRC).convert("RGBA")
    bg = im.getpixel((0, 0))[:3]
    pix = im.load()
    for y in range(im.height):
        for x in range(im.width):
            r, g, b, a = pix[x, y]
            if is_bg(r, g, b, a, bg):
                pix[x, y] = (0, 0, 0, 0)
    return remove_green_fringe(im)


def trim_alpha(im):
    bbox = im.getbbox()
    return im.crop(bbox) if bbox else Image.new("RGBA", (1, 1), (0, 0, 0, 0))


def split_frames(im):
    bbox = im.getbbox()
    if not bbox:
        return []
    minx, miny, maxx, maxy = bbox
    # The source contains the three substrate stages left-to-right. These cuts
    # keep small detached flakes with their nearest large pile.
    cuts = (393, 877)
    boxes = (
        (minx, miny, cuts[0], maxy),
        (cuts[0], miny, cuts[1], maxy),
        (cuts[1], miny, maxx, maxy),
    )
    return [trim_alpha(im.crop(box)) for box in boxes]


def make_frames():
    im = transparentized()
    crops = split_frames(im)
    frames = []
    for i, crop in enumerate(crops):
        target_h = TARGET_HEIGHTS[i]
        target_w = max(1, round(crop.width * (target_h / crop.height)))
        resized = crop.resize((target_w, target_h), Image.Resampling.LANCZOS)
        resized = remove_green_fringe(resized)
        resized = trim_alpha(resized)
        frames.append(resized)
        resized.save(EXTRACTED / f"substrate_{i}.png")
        print(f"substrate_{i}: {crop.width}x{crop.height} -> {resized.width}x{resized.height}")
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
    gap = 6
    w = sum(frame.width for frame in frames) + gap * (len(frames) - 1)
    h = max(frame.height for frame in frames)
    sheet = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    x = 0
    for frame in frames:
        sheet.alpha_composite(frame, (x, h - frame.height))
        x += frame.width + gap
    sheet.save(PREVIEWS / "substrate_sheet_preview.png")


def write_outputs(frames):
    OUT_H.write_text(
        """#pragma once
#include <Arduino.h>
#include <cstdint>

namespace SubstrateAssets {

static constexpr uint8_t FRAME_COUNT = 3;

struct RleFrame {
    uint16_t offset;
    uint16_t length;
    uint8_t width;
    uint8_t height;
};

extern const RleFrame FRAMES[] PROGMEM;
extern const uint16_t RLE[] PROGMEM;

}
""",
        encoding="utf-8",
    )
    data = []
    metas = []
    for frame in frames:
        enc = encode(frame)
        metas.append((len(data), len(enc), frame.width, frame.height))
        data.extend(enc)
    cpp = ['#include "SubstrateAssets.h"', "", "namespace SubstrateAssets {", ""]
    cpp.append("const RleFrame FRAMES[] PROGMEM = {")
    for off, length, width, height in metas:
        cpp.append(f"    {{ {off}, {length}, {width}, {height} }},")
    cpp.append("};")
    cpp.append("")
    cpp.append("const uint16_t RLE[] PROGMEM = {")
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
