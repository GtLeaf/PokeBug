#!/usr/bin/env python3
from collections import deque
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "asset/mainScene/beetle/hercules/adult"
OUT_H = ROOT / "src/assets/HerculesAdultSprites.h"
OUT_CPP = ROOT / "src/assets/HerculesAdultSprites.cpp"
SPARE = ROOT / "spareAsset"
EXTRACTED = SPARE / "extracted/beetle/hercules/adult"
PREVIEWS = SPARE / "previews/beetle/hercules/adult"
GENERATED = SPARE / "generated/src_assets"

SETS = [
    ("WALK", "walk.png"),
    ("EAT", "eat.png"),
    ("TURN", "turn.png"),
    ("THREATEN", "threaten.png"),
    ("RESET", "reset.png"),
]

POLICY = {
    "WALK": {"max_w": 48, "max_h": 28},
    "EAT": {"max_w": 50, "max_h": 22},
    "TURN": {"max_w": 34, "max_h": 30},
    # Threaten is body-scale driven: long/raised poses may use more height,
    # but all three frames keep a comparable body width.
    "THREATEN": {"max_w": 58, "max_h": 40},
    "RESET": {"max_w": 44, "max_h": 22},
}


def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def is_bg(r, g, b, a, bg):
    if a < 80:
        return True
    green = g > 145 and r < 155 and b < 155 and g > r * 1.35 and g > b * 1.35
    if green:
        return True
    grayish = abs(r - g) < 14 and abs(g - b) < 14 and abs(r - b) < 14 and r > 34
    if grayish:
        return True
    return abs(r - bg[0]) < 8 and abs(g - bg[1]) < 8 and abs(b - bg[2]) < 8


def transparentized(path):
    im = Image.open(path).convert("RGBA")
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


def components(path):
    im, mask = transparentized(path)
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
            if count > 100:
                comps.append((minx, miny, maxx + 1, maxy + 1))
    comps.sort(key=lambda b: (b[0], b[1]))
    return im, comps


def trim_alpha(im):
    bbox = im.getbbox()
    return im.crop(bbox) if bbox else Image.new("RGBA", (1, 1), (0, 0, 0, 0))


def make_frames(name, filename):
    im, comps = components(SRC / filename)
    policy = POLICY[name]
    frames = []
    for i, (x1, y1, x2, y2) in enumerate(comps):
        crop = trim_alpha(im.crop((x1, y1, x2, y2)))
        scale = min(policy["max_w"] / crop.width, policy["max_h"] / crop.height)
        resized = crop.resize(
            (max(1, round(crop.width * scale)), max(1, round(crop.height * scale))),
            Image.Resampling.LANCZOS,
        )
        resized = trim_alpha(resized)
        frames.append(resized)
        resized.save(EXTRACTED / f"{name.lower()}_{i}.png")
        print(f"{name.lower()}_{i}: {crop.width}x{crop.height} -> {resized.width}x{resized.height}")
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


def write_preview(all_sets):
    cell_w = 76
    cell_h = 56
    rows = []
    for name, _ in SETS:
        row = Image.new("RGBA", (cell_w * max(len(all_sets[name]), 1), cell_h), (0, 0, 0, 0))
        d = ImageDraw.Draw(row)
        d.line((0, 44, row.width, 44), fill=(255, 0, 0, 180), width=1)
        for i, fr in enumerate(all_sets[name]):
            x = i * cell_w + (cell_w - fr.width) // 2
            y = 44 - fr.height
            row.alpha_composite(fr, (x, y))
            d.rectangle((i * cell_w, 0, (i + 1) * cell_w - 1, cell_h - 1), outline=(120, 120, 120, 255))
            d.text((i * cell_w + 2, 2), f"{name.lower()} {fr.width}x{fr.height}", fill=(255, 255, 0, 255))
        rows.append(row)
    sheet = Image.new("RGBA", (max(r.width for r in rows), cell_h * len(rows)), (180, 180, 180, 255))
    for ri, row in enumerate(rows):
        sheet.alpha_composite(row, (0, ri * cell_h))
    sheet.save(PREVIEWS / "hercules_adult_variable_sheet_preview.png")


def write_outputs(all_sets):
    max_w = max(fr.width for frames in all_sets.values() for fr in frames)
    max_h = max(fr.height for frames in all_sets.values() for fr in frames)
    OUT_H.write_text(
        f"""#pragma once
#include <Arduino.h>
#include <cstdint>

namespace HerculesAdultSprites {{

static constexpr uint8_t MAX_FRAME_W = {max_w};
static constexpr uint8_t MAX_FRAME_H = {max_h};

struct RleFrame {{
    uint16_t offset;
    uint16_t length;
    uint8_t width;
    uint8_t height;
}};

extern const uint8_t WALK_FRAME_COUNT;
extern const RleFrame WALK_FRAMES[] PROGMEM;
extern const uint16_t WALK_RLE[] PROGMEM;

extern const uint8_t EAT_FRAME_COUNT;
extern const RleFrame EAT_FRAMES[] PROGMEM;
extern const uint16_t EAT_RLE[] PROGMEM;

extern const uint8_t TURN_FRAME_COUNT;
extern const RleFrame TURN_FRAMES[] PROGMEM;
extern const uint16_t TURN_RLE[] PROGMEM;

extern const uint8_t THREATEN_FRAME_COUNT;
extern const RleFrame THREATEN_FRAMES[] PROGMEM;
extern const uint16_t THREATEN_RLE[] PROGMEM;

extern const uint8_t RESET_FRAME_COUNT;
extern const RleFrame RESET_FRAMES[] PROGMEM;
extern const uint16_t RESET_RLE[] PROGMEM;

}}
""",
        encoding="utf-8",
    )

    cpp = ['#include "HerculesAdultSprites.h"', "", "namespace HerculesAdultSprites {", ""]
    for name, _ in SETS:
        data = []
        metas = []
        for fr in all_sets[name]:
            enc = encode(fr)
            metas.append((len(data), len(enc), fr.width, fr.height))
            data.extend(enc)
        cpp.append(f"const uint8_t {name}_FRAME_COUNT = {len(metas)};")
        cpp.append(f"const RleFrame {name}_FRAMES[] PROGMEM = {{")
        for off, length, w, h in metas:
            cpp.append(f"    {{ {off}, {length}, {w}, {h} }},")
        cpp.append("};")
        cpp.append("")
        cpp.append(f"const uint16_t {name}_RLE[] PROGMEM = {{")
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
    all_sets = {name: make_frames(name, filename) for name, filename in SETS}
    write_preview(all_sets)
    write_outputs(all_sets)
    (GENERATED / OUT_H.name).write_text(OUT_H.read_text(encoding="utf-8"), encoding="utf-8")
    (GENERATED / OUT_CPP.name).write_text(OUT_CPP.read_text(encoding="utf-8"), encoding="utf-8")


if __name__ == "__main__":
    main()
