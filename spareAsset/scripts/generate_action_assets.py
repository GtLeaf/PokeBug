#!/usr/bin/env python3
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "asset/mainScene/action/finger.png"
OUT_H = ROOT / "src/assets/ActionAssets.h"
OUT_CPP = ROOT / "src/assets/ActionAssets.cpp"
SPARE = ROOT / "spareAsset"
EXTRACTED = SPARE / "extracted/action/finger"
PREVIEWS = SPARE / "previews/action"
GENERATED = SPARE / "generated/src_assets"

ANGLES = [-14, 0, 14]
TARGET_H = 27


def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def remove_green(im):
    im = im.convert("RGBA")
    pix = im.load()
    for y in range(im.height):
        for x in range(im.width):
            r, g, b, a = pix[x, y]
            green = g > 150 and r < 140 and b < 140 and g > r * 1.6 and g > b * 1.6
            if a < 80 or green:
                pix[x, y] = (0, 0, 0, 0)
    return im.crop(im.getbbox())


def visible_bbox(im):
    pixels = im.load()
    xs = []
    ys = []
    for y in range(im.height):
        for x in range(im.width):
            if pixels[x, y][3] >= 80:
                xs.append(x)
                ys.append(y)
    return min(xs), min(ys), max(xs) + 1, max(ys) + 1


def tip_anchor(im):
    pixels = im.load()
    x1, y1, x2, y2 = visible_bbox(im)
    tip_x = x2 - 1
    edge_ys = [y for y in range(y1, y2) if pixels[tip_x, y][3] >= 80]
    if not edge_ys:
        edge_ys = [y for y in range(y1, y2) for x in range(max(x1, tip_x - 2), x2) if pixels[x, y][3] >= 80]
    tip_y = edge_ys[len(edge_ys) // 2] if edge_ys else (y1 + y2) // 2
    return tip_x, tip_y


def make_frames():
    cut = remove_green(Image.open(SRC))
    base = cut.resize((max(1, round(cut.width * TARGET_H / cut.height)), TARGET_H), Image.Resampling.LANCZOS)
    frames = []
    for angle in ANGLES:
        rotated = base.rotate(angle, resample=Image.Resampling.BICUBIC, expand=True)
        rotated = rotated.crop(rotated.getbbox())
        x1, y1, x2, y2 = visible_bbox(rotated)
        frame = rotated.crop((x1, y1, x2, y2))
        tip_x, tip_y = tip_anchor(frame)
        frames.append((frame, tip_x, tip_y))
        frame.save(EXTRACTED / f"finger_angle_{angle:+d}.png")
        print(f"finger {angle:+d}: {frame.width}x{frame.height} tip=({tip_x},{tip_y})")
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
    width = sum(frame.width for frame, _, _ in frames) + 8 * (len(frames) - 1)
    height = max(frame.height for frame, _, _ in frames)
    sheet = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    x = 0
    for frame, _, _ in frames:
        sheet.alpha_composite(frame, (x, height - frame.height))
        x += frame.width + 8
    sheet.save(PREVIEWS / "finger_angles_sheet_preview.png")


def write_outputs(frames):
    max_w = max(frame.width for frame, _, _ in frames)
    max_h = max(frame.height for frame, _, _ in frames)
    OUT_H.write_text(
        f"""#pragma once
#include <Arduino.h>
#include <cstdint>

namespace ActionAssets {{

static constexpr uint8_t FINGER_MAX_FRAME_W = {max_w};
static constexpr uint8_t FINGER_MAX_FRAME_H = {max_h};

struct RleFrame {{
    uint16_t offset;
    uint16_t length;
    uint8_t width;
    uint8_t height;
    uint8_t tipX;
    uint8_t tipY;
}};

extern const uint8_t FINGER_FRAME_COUNT;
extern const RleFrame FINGER_FRAMES[] PROGMEM;
extern const uint16_t FINGER_RLE[] PROGMEM;

}}
""",
        encoding="utf-8",
    )

    data = []
    metas = []
    for frame, tip_x, tip_y in frames:
        enc = encode(frame)
        metas.append((len(data), len(enc), frame.width, frame.height, tip_x, tip_y))
        data.extend(enc)
    cpp = ['#include "ActionAssets.h"', "", "namespace ActionAssets {", ""]
    cpp.append(f"const uint8_t FINGER_FRAME_COUNT = {len(metas)};")
    cpp.append("const RleFrame FINGER_FRAMES[] PROGMEM = {")
    for off, length, width, height, tip_x, tip_y in metas:
        cpp.append(f"    {{ {off}, {length}, {width}, {height}, {tip_x}, {tip_y} }},")
    cpp.append("};")
    cpp.append("")
    cpp.append("const uint16_t FINGER_RLE[] PROGMEM = {")
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
