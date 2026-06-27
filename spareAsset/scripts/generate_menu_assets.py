#!/usr/bin/env python3
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
SRC_CANDIDATES = (
    ROOT / "spareAsset/origin/menu/main_menu.png",
    ROOT / "spareAsset/origin/menu/menu_main.png",
)
BOX_SRC_CANDIDATES = (
    ROOT / "spareAsset/origin/menu/box/menu_box.png",
)
OUT_H = ROOT / "src/assets/MenuAssets.h"
OUT_CPP = ROOT / "src/assets/MenuAssets.cpp"
SPARE = ROOT / "spareAsset"
EXTRACTED = SPARE / "extracted/menu"
PREVIEWS = SPARE / "previews/menu"
GENERATED = SPARE / "generated/src_assets"

MENU_FRAME_W = 32
MENU_FRAME_H = 32
MENU_ICON_COUNT = 7
MENU_SHEET_W = MENU_FRAME_W * MENU_ICON_COUNT
MENU_SHEET_H = MENU_FRAME_H
MENU_SAFE_W = MENU_FRAME_W
MENU_SAFE_H = MENU_FRAME_H

SOURCE_FRAME_W = 16
SOURCE_FRAME_H = 16
SOURCE_GAP_W = 4
SOURCE_SHEET_W = MENU_ICON_COUNT * SOURCE_FRAME_W + (MENU_ICON_COUNT - 1) * SOURCE_GAP_W
SOURCE_SHEET_H = SOURCE_FRAME_H
SOURCE_GAP_COLOR = (0, 255, 0)


def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def is_bg(r, g, b, a, bg):
    if a < 80:
        return True
    close_to_corner = (
        abs(r - bg[0]) < 36 and
        abs(g - bg[1]) < 36 and
        abs(b - bg[2]) < 36
    )
    green_screen = g > 180 and r < 96 and b < 96
    return close_to_corner or green_screen


def transparentize_green(im):
    im = im.convert("RGBA")
    bg = im.getpixel((0, 0))[:3]
    pix = im.load()
    w, h = im.size
    seen = set()
    stack = []

    def is_connected_bg(x, y):
        r, g, b, a = pix[x, y]
        if is_bg(r, g, b, a, bg):
            return True
        return g > 120 and r < 180 and b < 180 and g - max(r, b) > 18

    for x in range(w):
        if is_connected_bg(x, 0):
            stack.append((x, 0))
        if is_connected_bg(x, h - 1):
            stack.append((x, h - 1))
    for y in range(h):
        if is_connected_bg(0, y):
            stack.append((0, y))
        if is_connected_bg(w - 1, y):
            stack.append((w - 1, y))

    while stack:
        x, y = stack.pop()
        if (x, y) in seen:
            continue
        if not is_connected_bg(x, y):
            continue

        seen.add((x, y))
        pix[x, y] = (0, 0, 0, 0)

        for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
            if 0 <= nx < w and 0 <= ny < h and (nx, ny) not in seen:
                stack.append((nx, ny))
    return im


def harden_alpha(im, threshold=128):
    im = im.convert("RGBA")
    pix = im.load()
    w, h = im.size
    for y in range(h):
        for x in range(w):
            r, g, b, a = pix[x, y]
            if a < threshold:
                pix[x, y] = (0, 0, 0, 0)
            else:
                pix[x, y] = (r, g, b, 255)
    return im


def remove_edge_shadow(im):
    im = im.convert("RGBA")
    pix = im.load()
    w, h = im.size
    clear = []
    neighbors = (
        (-1, -1), (0, -1), (1, -1),
        (-1, 0),           (1, 0),
        (-1, 1),  (0, 1),  (1, 1),
    )

    for y in range(h):
        for x in range(w):
            r, g, b, a = pix[x, y]
            if a < 80:
                continue

            touches_transparent = any(
                0 <= x + dx < w and
                0 <= y + dy < h and
                pix[x + dx, y + dy][3] < 80
                for dx, dy in neighbors
            )
            if not touches_transparent:
                continue

            max_rgb = max(r, g, b)
            min_rgb = min(r, g, b)
            saturation = max_rgb - min_rgb
            green_fringe = g > 130 and g - max(r, b) > 24
            shadow_fringe = (max_rgb < 210 and saturation < 42) or max_rgb < 130
            if green_fringe or shadow_fringe:
                clear.append((x, y))

    for x, y in clear:
        pix[x, y] = (0, 0, 0, 0)
    return im


def remove_connected_edge_green(im):
    im = im.convert("RGBA")
    pix = im.load()
    w, h = im.size
    seen = set()
    stack = []

    def is_green_artifact(x, y):
        r, g, b, a = pix[x, y]
        return a >= 80 and g > 100 and r < 200 and b < 200 and g - max(r, b) > 12

    def can_traverse(x, y):
        return pix[x, y][3] < 80 or is_green_artifact(x, y)

    for x in range(w):
        if can_traverse(x, 0):
            stack.append((x, 0))
        if can_traverse(x, h - 1):
            stack.append((x, h - 1))
    for y in range(h):
        if can_traverse(0, y):
            stack.append((0, y))
        if can_traverse(w - 1, y):
            stack.append((w - 1, y))

    while stack:
        x, y = stack.pop()
        if (x, y) in seen:
            continue
        if not can_traverse(x, y):
            continue

        seen.add((x, y))
        if is_green_artifact(x, y):
            pix[x, y] = (0, 0, 0, 0)

        for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
            if 0 <= nx < w and 0 <= ny < h and (nx, ny) not in seen:
                stack.append((nx, ny))
    return im


def remove_edge_warm_fringe(im, passes=3):
    im = im.convert("RGBA")
    pix = im.load()
    w, h = im.size
    neighbors = (
        (-1, -1), (0, -1), (1, -1),
        (-1, 0),           (1, 0),
        (-1, 1),  (0, 1),  (1, 1),
    )

    def is_warm_artifact(r, g, b, a):
        if a < 80:
            return False
        return (
            r <= 132 and g <= 88 and b <= 118 and
            r > g + 10 and
            b < r + 36
        )

    for _ in range(passes):
        clear = []
        for y in range(h):
            for x in range(w):
                r, g, b, a = pix[x, y]
                if not is_warm_artifact(r, g, b, a):
                    continue
                touches_transparent = any(
                    0 <= x + dx < w and
                    0 <= y + dy < h and
                    pix[x + dx, y + dy][3] < 80
                    for dx, dy in neighbors
                )
                if touches_transparent:
                    clear.append((x, y))

        if not clear:
            break
        for x, y in clear:
            pix[x, y] = (0, 0, 0, 0)
    return im


def trim_alpha(im):
    bbox = im.getbbox()
    return im.crop(bbox) if bbox else Image.new("RGBA", (1, 1), (0, 0, 0, 0))


def is_green_gap_pixel(r, g, b, a):
    return a >= 80 and abs(r - SOURCE_GAP_COLOR[0]) <= 8 and abs(g - SOURCE_GAP_COLOR[1]) <= 8 and abs(b - SOURCE_GAP_COLOR[2]) <= 8


def validate_source_gaps(source):
    pix = source.load()
    for gap in range(MENU_ICON_COUNT - 1):
        left = (gap + 1) * SOURCE_FRAME_W + gap * SOURCE_GAP_W
        for y in range(SOURCE_SHEET_H):
            for x in range(left, left + SOURCE_GAP_W):
                r, g, b, a = pix[x, y]
                if not is_green_gap_pixel(r, g, b, a):
                    raise ValueError(
                        f"Expected #00ff00 separator at x={x}, y={y} in {SOURCE_SHEET_W}x{SOURCE_SHEET_H} menu source"
                    )


def find_content_x_ranges(source):
    transparent = transparentize_green(source)
    pix = transparent.load()
    ranges = []
    start = None
    for x in range(transparent.width):
        has_content = False
        for y in range(transparent.height):
            if pix[x, y][3] >= 80:
                has_content = True
                break
        if has_content and start is None:
            start = x
        elif not has_content and start is not None:
            ranges.append((start, x))
            start = None
    if start is not None:
        ranges.append((start, transparent.width))

    merged = []
    for left, right in ranges:
        if merged and left - merged[-1][1] <= 2:
            merged[-1] = (merged[-1][0], right)
        else:
            merged.append((left, right))
    return merged


def make_source_tiles(source):
    if source.size == (SOURCE_SHEET_W, SOURCE_SHEET_H):
        validate_source_gaps(source)
        return [
            source.crop((
                i * (SOURCE_FRAME_W + SOURCE_GAP_W),
                0,
                i * (SOURCE_FRAME_W + SOURCE_GAP_W) + SOURCE_FRAME_W,
                SOURCE_FRAME_H,
            ))
            for i in range(MENU_ICON_COUNT)
        ]

    if source.size == (MENU_SHEET_W, MENU_SHEET_H):
        return [
            source.crop((i * MENU_FRAME_W, 0, (i + 1) * MENU_FRAME_W, MENU_FRAME_H))
            for i in range(MENU_ICON_COUNT)
        ]

    content_ranges = find_content_x_ranges(source)
    if len(content_ranges) == MENU_ICON_COUNT:
        return [
            source.crop((left, 0, right, source.height))
            for left, right in content_ranges
        ]

    tiles = []
    for i in range(MENU_ICON_COUNT):
        left = round(i * source.width / MENU_ICON_COUNT)
        right = round((i + 1) * source.width / MENU_ICON_COUNT)
        tiles.append(source.crop((left, 0, right, source.height)))
    return tiles


def make_frames(src_candidates=SRC_CANDIDATES, prefix="menu_main"):
    src = next((p for p in src_candidates if p.exists()), None)
    if src is None:
        candidates = ", ".join(str(p) for p in src_candidates)
        raise FileNotFoundError(f"No menu source found. Expected one of: {candidates}")

    source = Image.open(src).convert("RGBA")
    tiles = make_source_tiles(source)

    frames = []
    for i, tile in enumerate(tiles):
        tile = transparentize_green(tile)
        crop = trim_alpha(tile)
        scale = min(MENU_SAFE_W / crop.width, MENU_SAFE_H / crop.height)
        source_size = crop.size
        resample = Image.Resampling.NEAREST if source_size == (SOURCE_FRAME_W, SOURCE_FRAME_H) else Image.Resampling.LANCZOS
        resized = crop.resize(
            (max(1, round(crop.width * scale)), max(1, round(crop.height * scale))),
            resample,
        )
        resized = trim_alpha(remove_edge_shadow(harden_alpha(resized, 150)))

        canvas = Image.new("RGBA", (MENU_FRAME_W, MENU_FRAME_H), (0, 0, 0, 0))
        canvas.alpha_composite(
            resized,
            ((MENU_FRAME_W - resized.width) // 2, (MENU_FRAME_H - resized.height) // 2),
        )
        canvas = remove_connected_edge_green(canvas)
        if prefix == "menu_box" and i == 6:
            canvas = remove_edge_warm_fringe(canvas)
        frames.append(canvas)
        canvas.save(EXTRACTED / f"{prefix}_{i}.png")
        bbox = canvas.getbbox()
        bbox_size = (bbox[2] - bbox[0], bbox[3] - bbox[1]) if bbox else (0, 0)
        print(
            f"{prefix}_{i}: source {source_size[0]}x{source_size[1]} -> "
            f"safe {MENU_SAFE_W}x{MENU_SAFE_H}, frame {MENU_FRAME_W}x{MENU_FRAME_H}, "
            f"visible {bbox_size[0]}x{bbox_size[1]}"
        )
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


def write_preview(frames, name="menu_main_sheet_preview.png"):
    sheet = Image.new("RGBA", (MENU_FRAME_W * len(frames), MENU_FRAME_H), (0, 0, 0, 0))
    for i, frame in enumerate(frames):
        sheet.alpha_composite(frame, (i * MENU_FRAME_W, 0))
    sheet.save(PREVIEWS / name)


def encode_frames(frames):
    data = []
    metas = []
    for frame in frames:
        enc = encode(frame)
        metas.append((len(data), len(enc)))
        data.extend(enc)
    return metas, data


def write_outputs(main_frames, box_frames):
    OUT_H.write_text(
        f"""#pragma once
#include <Arduino.h>
#include <cstdint>

namespace MenuAssets {{

static constexpr uint8_t FRAME_W = {MENU_FRAME_W};
static constexpr uint8_t FRAME_H = {MENU_FRAME_H};
static constexpr uint8_t MAIN_ICON_COUNT = {len(main_frames)};
static constexpr uint8_t BOX_ICON_COUNT = {len(box_frames)};

struct RleFrame {{
    uint16_t offset;
    uint16_t length;
}};

extern const RleFrame MAIN_ICON_FRAMES[] PROGMEM;
extern const uint16_t MAIN_ICON_RLE[] PROGMEM;
extern const RleFrame BOX_ICON_FRAMES[] PROGMEM;
extern const uint16_t BOX_ICON_RLE[] PROGMEM;

}}
""",
        encoding="utf-8",
    )

    main_metas, main_data = encode_frames(main_frames)
    box_metas, box_data = encode_frames(box_frames)

    cpp = ['#include "MenuAssets.h"', "", "namespace MenuAssets {", ""]
    cpp.append("const RleFrame MAIN_ICON_FRAMES[] PROGMEM = {")
    for off, length in main_metas:
        cpp.append(f"    {{ {off}, {length} }},")
    cpp.append("};")
    cpp.append("")
    cpp.append("const uint16_t MAIN_ICON_RLE[] PROGMEM = {")
    cpp.append(fmt(main_data))
    cpp.append("};")
    cpp.append("")
    cpp.append("const RleFrame BOX_ICON_FRAMES[] PROGMEM = {")
    for off, length in box_metas:
        cpp.append(f"    {{ {off}, {length} }},")
    cpp.append("};")
    cpp.append("")
    cpp.append("const uint16_t BOX_ICON_RLE[] PROGMEM = {")
    cpp.append(fmt(box_data))
    cpp.append("};")
    cpp.append("")
    cpp.append("}")
    cpp.append("")
    OUT_CPP.write_text("\n".join(cpp), encoding="utf-8")


def main():
    EXTRACTED.mkdir(parents=True, exist_ok=True)
    PREVIEWS.mkdir(parents=True, exist_ok=True)
    GENERATED.mkdir(parents=True, exist_ok=True)
    main_frames = make_frames(SRC_CANDIDATES, "menu_main")
    box_frames = make_frames(BOX_SRC_CANDIDATES, "menu_box")
    write_preview(main_frames, "menu_main_sheet_preview.png")
    write_preview(box_frames, "menu_box_sheet_preview.png")
    write_outputs(main_frames, box_frames)
    (GENERATED / OUT_H.name).write_text(OUT_H.read_text(encoding="utf-8"), encoding="utf-8")
    (GENERATED / OUT_CPP.name).write_text(OUT_CPP.read_text(encoding="utf-8"), encoding="utf-8")


if __name__ == "__main__":
    main()
