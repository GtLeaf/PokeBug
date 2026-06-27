#!/usr/bin/env python3
import re
from collections import deque
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "asset/mainScene/beetle/hercules/adult"
ORIGIN = ROOT / "spareAsset/origin/beetle/hercules/adult"
ATTACK_ORIGIN = ORIGIN / "attack"
OUT_H = ROOT / "src/assets/HerculesAdultSprites.h"
OUT_CPP = ROOT / "src/assets/HerculesAdultSprites.cpp"
SPARE = ROOT / "spareAsset"
EXTRACTED = SPARE / "extracted/beetle/hercules/adult"
PREVIEWS = SPARE / "previews/beetle/hercules/adult"
GENERATED = SPARE / "generated/src_assets"

ACTION_SOURCES = [
    # kind="frames_dir" reads one PNG per frame. New walk/eat/sleep source
    # frames live under spareAsset/origin so future replacement only needs
    # swapping those PNGs and rerunning this script.
    {"name": "WALK", "kind": "frames_dir", "dir": ORIGIN / "walk"},
    {"name": "EAT", "kind": "frames_dir", "dir": ORIGIN / "eat"},
    {"name": "SLEEP_GETDOWN", "kind": "frames_dir", "dir": ORIGIN / "sleep/getDown", "scale_group": "SLEEP"},
    {"name": "SLEEP_BREATH", "kind": "frames_dir", "dir": ORIGIN / "sleep/breath", "scale_group": "SLEEP"},
    {"name": "TURN", "kind": "frames_dir", "dir": ORIGIN / "turn", "target_sizes": [(57, 48), (56, 49)]},
    {"name": "THREATEN", "kind": "frames_dir", "dir": ORIGIN / "threaten"},
    {"name": "ATTACK_DOWN", "kind": "frames_dir", "dir": ATTACK_ORIGIN / "down", "scale_group": "ATTACK"},
    {"name": "ATTACK_UP", "kind": "frames_dir", "dir": ATTACK_ORIGIN / "up", "scale_group": "ATTACK"},
    {"name": "RESET", "kind": "frames_dir", "dir": ORIGIN / "sleep/breath", "frame_indices": [2], "scale_group": "SLEEP"},
]

ACTION_NAMES = [source["name"] for source in ACTION_SOURCES]

# Size policy.
#
# Usage for future size tuning:
# 1. Change ADULT_BASE_SCALE to resize every adult action together while keeping
#    each action's relative proportions. Example:
#       ADULT_BASE_SCALE = 1.10  # all adult sprites become 10% larger
#       ADULT_BASE_SCALE = 0.90  # all adult sprites become 10% smaller
# 2. Keep ADULT_RUNTIME_MAX_SCALE in sync with Bug::getAdultScale() max value.
#    The script exports terrarium movement bounds for that worst-case size.
# 3. Rerun:
#       python3 spareAsset/scripts/generate_hercules_adult_sprites.py
#    Then build with:
#       pio run
#
# BASE_POLICY stores the current hand-tuned visual targets at 1.0x. These
# numbers are based on WALK as the body-size reference; do not make every frame
# independently fill the canvas, or low/raised poses will look like different
# sized beetles.
ADULT_BASE_SCALE = 1.73
ADULT_RUNTIME_MAX_SCALE = 1.20
SCREEN_WIDTH = 240
# Exported frames keep transparent safety margin. The horn tip often lands on
# the right edge after tight cropping; padding prevents right-facing sprites
# from reading as clipped in-game.
FRAME_PAD_X = 6
FRAME_PAD_Y = 3

BASE_POLICY = {
    "WALK": {"max_w": 48, "max_h": 28},
    "EAT": {"max_w": 50, "max_h": 22},
    "SLEEP_GETDOWN": {"max_w": 50, "max_h": 24},
    "SLEEP_BREATH": {"max_w": 50, "max_h": 24},
    "TURN": {"max_w": 34, "max_h": 30},
    # Threaten is body-scale driven: long/raised poses may use more height,
    # but all three frames keep a comparable body width.
    "THREATEN": {"max_w": 50, "max_h": 40},
    # Battle attack frames are generated from large transparent source frames.
    # Keep down/up in one scale group so the wind-up and upward horn lift read
    # as one continuous body size.
    "ATTACK_DOWN": {"max_w": 54, "max_h": 34},
    "ATTACK_UP": {"max_w": 54, "max_h": 34},
    "RESET": {"max_w": 50, "max_h": 24},
}

# Palette replacement key written into generated RLE.
#
# Usage:
# 1. Art can keep using the current full-color beetle PNGs.
# 2. Only the deliberate palette-red source pixels are classified before
#    resizing. Current exported art stores the #ff0000 guide as very close reds
#    like #ff1d06, so the seed rule requires a very strong red channel with low
#    G/B. Beetle body, shadows, outline, legs, gradients, and red-brown
#    antialiasing remain original art.
# 3. After resizing, the exact-red mask is also resized softly so sampled
#    palette pixels do not become blurry/near-red. Those pixels are normalized
#    back to exact #ff0000 (RGB565 0xF800), and runtime code replaces only this
#    palette key.
# 4. To tune future art, edit classify_pixel() below and rerun:
#       python3 spareAsset/scripts/generate_hercules_adult_sprites.py
PALETTE_RGB = (255, 0, 0, 255)
PALETTE_SOFT_MASK_THRESHOLD = 4

CLASS_NONE = 0
CLASS_PALETTE = 1


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


def classify_pixel(r, g, b, a):
    """Return palette class for one visible source pixel.

    Palette control is deliberate: only the near-#ff0000 guide color is treated
    as a runtime replaceable palette key. A later local edge pass recovers
    one-pixel sampled fringes that touch confirmed palette pixels.
    Use the soft mask after scaling to recover sampled pixels around those key
    pixels.
    """
    if a < 80:
        return CLASS_NONE

    if r >= 240 and g <= 45 and b <= 14:
        return CLASS_PALETTE

    return CLASS_NONE


def transparentized(path):
    im = Image.open(path).convert("RGBA")
    bg = im.getpixel((0, 0))[:3]
    pix = im.load()
    w, h = im.size
    mask = [[False] * w for _ in range(h)]
    classes = Image.new("L", (w, h), CLASS_NONE)
    class_pix = classes.load()
    for y in range(h):
        for x in range(w):
            r, g, b, a = pix[x, y]
            if is_bg(r, g, b, a, bg):
                pix[x, y] = (0, 0, 0, 0)
            else:
                mask[y][x] = True
                class_pix[x, y] = classify_pixel(r, g, b, a)
    return im, mask, classes


def components(path):
    im, mask, classes = transparentized(path)
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
    return im, classes, comps


def natural_key(path):
    return [int(part) if part.isdigit() else part.lower() for part in re.split(r"(\d+)", path.name)]


def image_data(img):
    getter = getattr(img, "get_flattened_data", None)
    return getter() if getter else img.getdata()


def make_scaled_policy(base_scale):
    return {
        name: {
            "max_w": max(1, round(value["max_w"] * base_scale)),
            "max_h": max(1, round(value["max_h"] * base_scale)),
        }
        for name, value in BASE_POLICY.items()
    }


def terrarium_bounds(max_frame_w):
    half_width = max(1, round(max_frame_w * ADULT_RUNTIME_MAX_SCALE / 2))
    return {
        "half_width": half_width,
        "min_x": half_width,
        "max_x": SCREEN_WIDTH - half_width,
    }


def visible_bbox(mask):
    minx = miny = None
    maxx = maxy = 0
    for y, row in enumerate(mask):
        for x, visible in enumerate(row):
            if not visible:
                continue
            if minx is None:
                minx = maxx = x
                miny = maxy = y
            else:
                minx = min(minx, x)
                miny = min(miny, y)
                maxx = max(maxx, x)
                maxy = max(maxy, y)
    return None if minx is None else (minx, miny, maxx + 1, maxy + 1)


def trim_alpha(im):
    bbox = im.getbbox()
    return im.crop(bbox) if bbox else Image.new("RGBA", (1, 1), (0, 0, 0, 0))


def palette_mask_from_classes(class_img):
    return class_img.point(lambda v: 255 if v == CLASS_PALETTE else 0)


def is_palette_edge_pixel(r, g, b, a):
    if a < 80:
        return False
    return r >= 170 and g <= 95 and b <= 80 and r > g * 1.6 and r > b * 1.6


def apply_palette_key(img, class_img, soft_palette_mask):
    keyed = img.copy()
    pix = keyed.load()
    class_pix = class_img.load()
    soft_pix = soft_palette_mask.load()
    for y in range(keyed.height):
        for x in range(keyed.width):
            if pix[x, y][3] < 80:
                continue
            cls = class_pix[x, y]
            if cls == CLASS_PALETTE:
                pix[x, y] = PALETTE_RGB
                continue
            r, g, b, a = pix[x, y]
            if soft_pix[x, y] >= PALETTE_SOFT_MASK_THRESHOLD and is_palette_edge_pixel(r, g, b, a):
                pix[x, y] = PALETTE_RGB
    edge_points = []
    for y in range(keyed.height):
        for x in range(keyed.width):
            r, g, b, a = pix[x, y]
            if not is_palette_edge_pixel(r, g, b, a):
                continue
            for ny in range(max(0, y - 1), min(keyed.height, y + 2)):
                for nx in range(max(0, x - 1), min(keyed.width, x + 2)):
                    if pix[nx, ny] == PALETTE_RGB:
                        edge_points.append((x, y))
                        break
                else:
                    continue
                break
    for x, y in edge_points:
        pix[x, y] = PALETTE_RGB
    return keyed


def pad_frame(img):
    if FRAME_PAD_X <= 0 and FRAME_PAD_Y <= 0:
        return img
    padded = Image.new(
        "RGBA",
        (img.width + FRAME_PAD_X * 2, img.height + FRAME_PAD_Y * 2),
        (0, 0, 0, 0),
    )
    padded.alpha_composite(img, (FRAME_PAD_X, FRAME_PAD_Y))
    return padded


def source_path(filename):
    path = Path(filename)
    return path if path.is_absolute() else SRC / path


def build_frame(name, frame_index, raw_crop, raw_classes, source_label, scale, target_size=None):
    bbox = raw_crop.getbbox()
    crop = raw_crop.crop(bbox) if bbox else Image.new("RGBA", (1, 1), (0, 0, 0, 0))
    class_crop = raw_classes.crop(bbox) if bbox else Image.new("L", (1, 1), CLASS_NONE)
    resized_size = target_size or (
        max(1, round(crop.width * scale)),
        max(1, round(crop.height * scale)),
    )
    resized = crop.resize(
        resized_size,
        Image.Resampling.LANCZOS,
    )
    resized_classes = class_crop.resize(resized.size, Image.Resampling.NEAREST)
    resized_soft_palette = palette_mask_from_classes(class_crop).resize(resized.size, Image.Resampling.LANCZOS)
    bbox = resized.getbbox()
    resized = resized.crop(bbox) if bbox else resized
    resized_classes = resized_classes.crop(bbox) if bbox else resized_classes
    resized_soft_palette = resized_soft_palette.crop(bbox) if bbox else resized_soft_palette
    keyed = pad_frame(apply_palette_key(resized, resized_classes, resized_soft_palette))
    keyed.save(EXTRACTED / f"{name.lower()}_{frame_index}.png")
    palette_pixels = sum(1 for v in image_data(resized_classes) if v == CLASS_PALETTE)
    print(
        f"{name.lower()}_{frame_index}: {source_label} {crop.width}x{crop.height} -> "
        f"{keyed.width}x{keyed.height} scale={scale:.5f} palette_pixels={palette_pixels}"
    )
    return keyed


def raw_frames_from_sheet(filename):
    path = source_path(filename)
    im, classes, comps = components(path)
    frames = []
    for i, (x1, y1, x2, y2) in enumerate(comps):
        raw_crop = im.crop((x1, y1, x2, y2))
        raw_classes = classes.crop((x1, y1, x2, y2))
        frames.append((raw_crop, raw_classes, path.name))
    return frames


def raw_frames_from_dir(frames_dir):
    files = sorted(frames_dir.glob("*.png"), key=natural_key)
    if not files:
        raise FileNotFoundError(f"No PNG frames found in {frames_dir}")

    frames = []
    for path in files:
        im, mask, classes = transparentized(path)
        bbox = visible_bbox(mask)
        if bbox:
            raw_crop = im.crop(bbox)
            raw_classes = classes.crop(bbox)
        else:
            raw_crop = Image.new("RGBA", (1, 1), (0, 0, 0, 0))
            raw_classes = Image.new("L", (1, 1), CLASS_NONE)
        frames.append((raw_crop, raw_classes, path.name))
    return frames


def make_raw_frames(source):
    kind = source["kind"]
    if kind == "sheet":
        return raw_frames_from_sheet(source["file"])
    if kind == "frames_dir":
        frames = raw_frames_from_dir(source["dir"])
        if "frame_indices" in source:
            picked = []
            for idx in source["frame_indices"]:
                if idx < 0 or idx >= len(frames):
                    raise IndexError(f"Frame index {idx} out of range for {source['dir']}")
                picked.append(frames[idx])
            return picked
        return frames
    raise ValueError(f"Unsupported action source kind: {kind}")


def scale_group(source):
    return source.get("scale_group", source["name"])


def compute_group_scales(raw_sets, policy):
    group_scales = {}
    for source in ACTION_SOURCES:
        name = source["name"]
        group = scale_group(source)
        size_policy = policy[name]
        for raw_crop, _raw_classes, _source_label in raw_sets[name]:
            bbox = raw_crop.getbbox()
            crop = raw_crop.crop(bbox) if bbox else Image.new("RGBA", (1, 1), (0, 0, 0, 0))
            frame_scale = min(size_policy["max_w"] / crop.width, size_policy["max_h"] / crop.height)
            if group not in group_scales:
                group_scales[group] = frame_scale
            else:
                group_scales[group] = min(group_scales[group], frame_scale)
    return group_scales


def make_frames(source, raw_sets, group_scales):
    name = source["name"]
    scale = group_scales[scale_group(source)]
    target_sizes = source.get("target_sizes")
    frames = []
    for i, (raw_crop, raw_classes, source_label) in enumerate(raw_sets[name]):
        target_size = target_sizes[i] if target_sizes and i < len(target_sizes) else None
        frames.append(build_frame(name, i, raw_crop, raw_classes, source_label, scale, target_size))
    return frames


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


def write_preview(all_sets):
    max_w = max(fr.width for frames in all_sets.values() for fr in frames)
    max_h = max(fr.height for frames in all_sets.values() for fr in frames)
    cell_w = max(76, max_w + 18)
    cell_h = max(56, max_h + 17)
    ground_y = cell_h - 12
    rows = []
    for name in ACTION_NAMES:
        row = Image.new("RGBA", (cell_w * max(len(all_sets[name]), 1), cell_h), (0, 0, 0, 0))
        d = ImageDraw.Draw(row)
        d.line((0, ground_y, row.width, ground_y), fill=(255, 0, 0, 180), width=1)
        for i, fr in enumerate(all_sets[name]):
            x = i * cell_w + (cell_w - fr.width) // 2
            y = ground_y - fr.height
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
    if max_w > 255 or max_h > 255:
        raise ValueError(f"Frame metadata uses uint8_t; generated max frame {max_w}x{max_h} is too large")
    bounds = terrarium_bounds(max_w)
    externs = []
    for name in ACTION_NAMES:
        externs.append(f"extern const uint8_t {name}_FRAME_COUNT;")
        externs.append(f"extern const RleFrame {name}_FRAMES[] PROGMEM;")
        externs.append(f"extern const uint16_t {name}_RLE[] PROGMEM;")
        externs.append("")
    OUT_H.write_text(
        f"""#pragma once
#include <Arduino.h>
#include <cstdint>

namespace HerculesAdultSprites {{

static constexpr uint16_t MAX_FRAME_W = {max_w};
static constexpr uint16_t MAX_FRAME_H = {max_h};
static constexpr uint8_t BASE_SCALE_PERCENT = {round(ADULT_BASE_SCALE * 100)};
static constexpr uint8_t RUNTIME_MAX_SCALE_PERCENT = {round(ADULT_RUNTIME_MAX_SCALE * 100)};
static constexpr uint8_t TERRARIUM_EDGE_HALF_W = {bounds["half_width"]};
static constexpr uint8_t TERRARIUM_MIN_X = {bounds["min_x"]};
static constexpr uint8_t TERRARIUM_MAX_X = {bounds["max_x"]};

static constexpr uint16_t PALETTE_KEY = 0xF800;

struct RleFrame {{
    uint16_t offset;
    uint16_t length;
    uint8_t width;
    uint8_t height;
}};

{chr(10).join(externs)}

}}
""",
        encoding="utf-8",
    )

    cpp = ['#include "HerculesAdultSprites.h"', "", "namespace HerculesAdultSprites {", ""]
    for name in ACTION_NAMES:
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
    policy = make_scaled_policy(ADULT_BASE_SCALE)
    print(f"ADULT_BASE_SCALE={ADULT_BASE_SCALE:.2f}")
    print(f"ADULT_RUNTIME_MAX_SCALE={ADULT_RUNTIME_MAX_SCALE:.2f}")
    print(f"POLICY={policy}")
    raw_sets = {source["name"]: make_raw_frames(source) for source in ACTION_SOURCES}
    group_scales = compute_group_scales(raw_sets, policy)
    print(f"GROUP_SCALES={group_scales}")
    all_sets = {source["name"]: make_frames(source, raw_sets, group_scales) for source in ACTION_SOURCES}
    write_preview(all_sets)
    write_outputs(all_sets)
    (GENERATED / OUT_H.name).write_text(OUT_H.read_text(encoding="utf-8"), encoding="utf-8")
    (GENERATED / OUT_CPP.name).write_text(OUT_CPP.read_text(encoding="utf-8"), encoding="utf-8")
    max_w = max(fr.width for frames in all_sets.values() for fr in frames)
    max_h = max(fr.height for frames in all_sets.values() for fr in frames)
    bounds = terrarium_bounds(max_w)
    print(
        f"MAX_FRAME={max_w}x{max_h}, "
        f"RUNTIME_MAX_HALF_WIDTH={bounds['half_width']}, "
        f"TERRARIUM_X={bounds['min_x']}..{bounds['max_x']}"
    )


if __name__ == "__main__":
    main()
