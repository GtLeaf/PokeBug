#!/usr/bin/env python3
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "spareAsset/legacy/menu/menu_main.png"
OUT_H = ROOT / "src/assets/MenuAssets.h"
OUT_CPP = ROOT / "src/assets/MenuAssets.cpp"
SPARE = ROOT / "spareAsset"
EXTRACTED = SPARE / "extracted/menu"
PREVIEWS = SPARE / "previews/menu"
GENERATED = SPARE / "generated/src_assets"

MENU_FRAME_W = 40
MENU_FRAME_H = 32
MENU_ICON_COUNT = 7


def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def is_bg(r, g, b, a, bg):
    if a < 80:
        return True
    close_to_corner = (
        abs(r - bg[0]) < 24 and
        abs(g - bg[1]) < 24 and
        abs(b - bg[2]) < 24
    )
    pure_green_spill = r < 32 and g > 220 and b < 32
    return close_to_corner or pure_green_spill


def transparentize_green(im):
    im = im.convert("RGBA")
    bg = im.getpixel((0, 0))[:3]
    pix = im.load()
    w, h = im.size
    for y in range(h):
        for x in range(w):
            r, g, b, a = pix[x, y]
            if is_bg(r, g, b, a, bg):
                pix[x, y] = (0, 0, 0, 0)
    return im


def trim_alpha(im):
    bbox = im.getbbox()
    return im.crop(bbox) if bbox else Image.new("RGBA", (1, 1), (0, 0, 0, 0))


def make_frames():
    source = transparentize_green(Image.open(SRC))
    tile_w = source.width // MENU_ICON_COUNT
    if tile_w * MENU_ICON_COUNT != source.width:
        raise ValueError(f"Expected {MENU_ICON_COUNT} equal menu tiles, got width {source.width}")

    frames = []
    for i in range(MENU_ICON_COUNT):
        tile = source.crop((i * tile_w, 0, (i + 1) * tile_w, source.height))
        crop = trim_alpha(tile)
        scale = min(MENU_FRAME_W / crop.width, MENU_FRAME_H / crop.height)
        resized = crop.resize(
            (max(1, round(crop.width * scale)), max(1, round(crop.height * scale))),
            Image.Resampling.LANCZOS,
        )
        resized = trim_alpha(resized)

        canvas = Image.new("RGBA", (MENU_FRAME_W, MENU_FRAME_H), (0, 0, 0, 0))
        canvas.alpha_composite(
            resized,
            ((MENU_FRAME_W - resized.width) // 2, (MENU_FRAME_H - resized.height) // 2),
        )
        frames.append(canvas)
        canvas.save(EXTRACTED / f"menu_main_{i}.png")
        print(f"menu_main_{i}: {crop.width}x{crop.height} -> {resized.width}x{resized.height}")
    return frames


def encode(img):
    pixels = list(img.getdata())
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
    sheet = Image.new("RGBA", (MENU_FRAME_W * len(frames), MENU_FRAME_H), (0, 0, 0, 0))
    for i, frame in enumerate(frames):
        sheet.alpha_composite(frame, (i * MENU_FRAME_W, 0))
    sheet.save(PREVIEWS / "menu_main_sheet_preview.png")


def write_outputs(frames):
    OUT_H.write_text(
        f"""#pragma once
#include <Arduino.h>
#include <cstdint>

namespace MenuAssets {{

static constexpr uint8_t FRAME_W = {MENU_FRAME_W};
static constexpr uint8_t FRAME_H = {MENU_FRAME_H};
static constexpr uint8_t MAIN_ICON_COUNT = {len(frames)};

struct RleFrame {{
    uint16_t offset;
    uint16_t length;
}};

extern const RleFrame MAIN_ICON_FRAMES[] PROGMEM;
extern const uint16_t MAIN_ICON_RLE[] PROGMEM;

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

    cpp = ['#include "MenuAssets.h"', "", "namespace MenuAssets {", ""]
    cpp.append("const RleFrame MAIN_ICON_FRAMES[] PROGMEM = {")
    for off, length in metas:
        cpp.append(f"    {{ {off}, {length} }},")
    cpp.append("};")
    cpp.append("")
    cpp.append("const uint16_t MAIN_ICON_RLE[] PROGMEM = {")
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
