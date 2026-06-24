#!/usr/bin/env python3
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
SRC_DIR = ROOT / "spareAsset/origin/beetle/hercules/larva"
OUT_H = ROOT / "src/assets/HerculesLarvaSprites.h"
OUT_CPP = ROOT / "src/assets/HerculesLarvaSprites.cpp"
SPARE = ROOT / "spareAsset"
EXTRACTED = SPARE / "extracted/beetle/hercules/larva"
PREVIEWS = SPARE / "previews/beetle/hercules/larva"
GENERATED = SPARE / "generated/src_assets"

# Usage:
# 1. Put larva images under this structure:
#      spareAsset/origin/beetle/hercules/larva/age1/idle/*.png
#      spareAsset/origin/beetle/hercules/larva/age1/sleep/frame_1.png
#      spareAsset/origin/beetle/hercules/larva/age1/sleep/frame_2.png
#      spareAsset/origin/beetle/hercules/larva/age1/sleep/frame_3.png
#      ... repeat for age2 and age3.
#      spareAsset/origin/beetle/hercules/larva/age1/eat/frame_1.png
#      spareAsset/origin/beetle/hercules/larva/age1/eat/frame_2.png
#      spareAsset/origin/beetle/hercules/larva/age1/eat/frame_3.png
#      spareAsset/origin/beetle/hercules/larva/age1/eat/frame_4.png
#      ... repeat eat frames for age2 and age3.
# 2. Each age has one idle frame, three sleep frames, and four eat frames.
#    Sleep/eat frames play only after entering that visual state.
# 3. Generated frames use BASE_FRAME_* multiplied by LARVA_SIZE_SCALE,
#    bottom-aligned. Idle/sleep share one scale per age; eat uses a separate
#    per-age scale so oversized food/bite poses do not shrink normal idle
#    growth frames. Change LARVA_SIZE_SCALE to resize all larva frames.
# 4. Run:
#      python3 spareAsset/scripts/generate_hercules_larva_sprites.py
#    Then build with:
#      pio run

AGE_COUNT = 3
SLEEP_FRAME_COUNT = 3
EAT_FRAME_COUNT = 4
BASE_FRAME_W = 76
BASE_FRAME_H = 52
LARVA_SIZE_SCALE = 1.2
LARVA_FRAME_W = round(BASE_FRAME_W * LARVA_SIZE_SCALE)
LARVA_FRAME_H = round(BASE_FRAME_H * LARVA_SIZE_SCALE)
LARVA_MAX_W = LARVA_FRAME_W
LARVA_MAX_H = LARVA_FRAME_H


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


def first_png(directory):
    candidates = sorted(directory.glob("*.png"))
    if not candidates:
        raise FileNotFoundError(f"No larva PNG found in {directory}")
    return candidates[0]


def clear_pngs(directory):
    if not directory.exists():
        return
    for path in directory.glob("*.png"):
        path.unlink()


def source_images():
    ages = []
    for age in range(1, AGE_COUNT + 1):
        age_dir = SRC_DIR / f"age{age}"
        idle_path = first_png(age_dir / "idle")
        sleep_paths = []
        for frame in range(1, SLEEP_FRAME_COUNT + 1):
            path = age_dir / "sleep" / f"frame_{frame}.png"
            if not path.exists():
                path = first_png(age_dir / "sleep")
            sleep_paths.append(path)
        eat_paths = []
        for frame in range(1, EAT_FRAME_COUNT + 1):
            path = age_dir / "eat" / f"frame_{frame}.png"
            if not path.exists():
                path = first_png(age_dir / "eat")
            eat_paths.append(path)
        ages.append((idle_path, sleep_paths, eat_paths))
    return ages


def make_canvas(crop, scale):
    resized = crop.resize(
        (max(1, round(crop.width * scale)), max(1, round(crop.height * scale))),
        Image.Resampling.LANCZOS,
    )
    resized = trim_alpha(remove_green_bg(resized))
    frame = Image.new("RGBA", (LARVA_FRAME_W, LARVA_FRAME_H), (0, 0, 0, 0))
    frame.alpha_composite(resized, ((LARVA_FRAME_W - resized.width) // 2, LARVA_FRAME_H - resized.height))
    return frame, resized


def make_frames():
    (EXTRACTED / "idle").mkdir(parents=True, exist_ok=True)
    (EXTRACTED / "sleep").mkdir(parents=True, exist_ok=True)
    (EXTRACTED / "eat").mkdir(parents=True, exist_ok=True)
    PREVIEWS.mkdir(parents=True, exist_ok=True)
    GENERATED.mkdir(parents=True, exist_ok=True)
    clear_pngs(EXTRACTED / "idle")
    clear_pngs(EXTRACTED / "sleep")
    clear_pngs(EXTRACTED / "eat")
    clear_pngs(PREVIEWS)

    idle_frames = []
    sleep_frames = []
    eat_frames = []
    for age, (idle_path, sleep_paths, eat_paths) in enumerate(source_images(), start=1):
        idle_sleep_paths = [("idle", 0, idle_path)]
        idle_sleep_paths.extend(("sleep", i, path) for i, path in enumerate(sleep_paths))
        eat_role_paths = [("eat", i, path) for i, path in enumerate(eat_paths)]

        idle_sleep_crops = []
        for role, role_index, path in idle_sleep_paths:
            source = Image.open(path).convert("RGBA")
            crop = trim_alpha(remove_green_bg(source))
            idle_sleep_crops.append((role, role_index, path, source, crop))

        eat_crops = []
        for role, role_index, path in eat_role_paths:
            source = Image.open(path).convert("RGBA")
            crop = trim_alpha(remove_green_bg(source))
            eat_crops.append((role, role_index, path, source, crop))

        idle_sleep_max_w = max(crop.width for _, _, _, _, crop in idle_sleep_crops)
        idle_sleep_max_h = max(crop.height for _, _, _, _, crop in idle_sleep_crops)
        idle_sleep_scale = min(LARVA_MAX_W / idle_sleep_max_w, LARVA_MAX_H / idle_sleep_max_h)

        eat_max_w = max(crop.width for _, _, _, _, crop in eat_crops)
        eat_max_h = max(crop.height for _, _, _, _, crop in eat_crops)
        eat_scale = min(LARVA_MAX_W / eat_max_w, LARVA_MAX_H / eat_max_h)

        for role, role_index, path, source, crop in idle_sleep_crops + eat_crops:
            scale = eat_scale if role == "eat" else idle_sleep_scale
            frame, resized = make_canvas(crop, scale)
            if role == "idle":
                idle_frames.append(frame)
                frame.save(EXTRACTED / "idle" / f"larva_age{age}_idle.png")
                label = f"larva_age{age}_idle"
            elif role == "sleep":
                sleep_frames.append(frame)
                frame.save(EXTRACTED / "sleep" / f"larva_age{age}_sleep_{role_index}.png")
                label = f"larva_age{age}_sleep_{role_index}"
            else:
                eat_frames.append(frame)
                frame.save(EXTRACTED / "eat" / f"larva_age{age}_eat_{role_index}.png")
                label = f"larva_age{age}_eat_{role_index}"
            print(
                f"{label}: source {source.width}x{source.height}, bbox {crop.width}x{crop.height} "
                f"-> {resized.width}x{resized.height} in {LARVA_FRAME_W}x{LARVA_FRAME_H} ({path.name})"
            )
    return idle_frames, sleep_frames, eat_frames


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


def write_preview(idle_frames, sleep_frames, eat_frames):
    idle_sheet = Image.new("RGBA", (LARVA_FRAME_W * len(idle_frames), LARVA_FRAME_H), (0, 0, 0, 0))
    for i, frame in enumerate(idle_frames):
        idle_sheet.alpha_composite(frame, (i * LARVA_FRAME_W, 0))
    idle_sheet.save(PREVIEWS / "larva_idle_sheet_preview.png")

    sleep_sheet = Image.new("RGBA", (LARVA_FRAME_W * len(sleep_frames), LARVA_FRAME_H), (0, 0, 0, 0))
    for i, frame in enumerate(sleep_frames):
        sleep_sheet.alpha_composite(frame, (i * LARVA_FRAME_W, 0))
    sleep_sheet.save(PREVIEWS / "larva_sleep_sheet_preview.png")

    eat_sheet = Image.new("RGBA", (LARVA_FRAME_W * len(eat_frames), LARVA_FRAME_H), (0, 0, 0, 0))
    for i, frame in enumerate(eat_frames):
        eat_sheet.alpha_composite(frame, (i * LARVA_FRAME_W, 0))
    eat_sheet.save(PREVIEWS / "larva_eat_sheet_preview.png")

    gap = 8
    compare_w = LARVA_FRAME_W * (1 + EAT_FRAME_COUNT + SLEEP_FRAME_COUNT) + gap * 2
    for age_index in range(AGE_COUNT):
        compare = Image.new("RGBA", (compare_w, LARVA_FRAME_H), (0, 0, 0, 0))
        x = 0
        compare.alpha_composite(idle_frames[age_index], (x, 0))
        x += LARVA_FRAME_W + gap
        for i in range(EAT_FRAME_COUNT):
            compare.alpha_composite(eat_frames[age_index * EAT_FRAME_COUNT + i], (x, 0))
            x += LARVA_FRAME_W
        x += gap
        for i in range(SLEEP_FRAME_COUNT):
            compare.alpha_composite(sleep_frames[age_index * SLEEP_FRAME_COUNT + i], (x, 0))
            x += LARVA_FRAME_W
        compare.save(PREVIEWS / f"larva_age{age_index + 1}_compare_preview.png")


def build_chunks(frames):
    chunks = []
    offset = 0
    meta = []
    for frame in frames:
        encoded = encode(frame)
        meta.append((offset, len(encoded), LARVA_FRAME_W, LARVA_FRAME_H))
        chunks.extend(encoded)
        offset += len(encoded)
    return meta, chunks


def write_outputs(idle_frames, sleep_frames, eat_frames):
    h = f"""#pragma once
#include <Arduino.h>
#include <cstdint>

namespace HerculesLarvaSprites {{

static constexpr uint8_t AGE_COUNT = {AGE_COUNT};
static constexpr uint8_t IDLE_FRAME_COUNT = {AGE_COUNT};
static constexpr uint8_t SLEEP_FRAME_COUNT = {SLEEP_FRAME_COUNT};
static constexpr uint8_t EAT_FRAME_COUNT = {EAT_FRAME_COUNT};
static constexpr uint8_t IDLE_FRAME_W = {LARVA_FRAME_W};
static constexpr uint8_t IDLE_FRAME_H = {LARVA_FRAME_H};
static constexpr uint8_t SLEEP_FRAME_W = {LARVA_FRAME_W};
static constexpr uint8_t SLEEP_FRAME_H = {LARVA_FRAME_H};
static constexpr uint8_t EAT_FRAME_W = {LARVA_FRAME_W};
static constexpr uint8_t EAT_FRAME_H = {LARVA_FRAME_H};

struct RleFrame {{
    uint16_t offset;
    uint16_t length;
    uint8_t width;
    uint8_t height;
}};

extern const RleFrame IDLE_FRAMES[] PROGMEM;
extern const uint16_t IDLE_RLE[] PROGMEM;
extern const RleFrame SLEEP_FRAMES[] PROGMEM;
extern const uint16_t SLEEP_RLE[] PROGMEM;
extern const RleFrame EAT_FRAMES[] PROGMEM;
extern const uint16_t EAT_RLE[] PROGMEM;

}}
"""
    OUT_H.write_text(h, encoding="utf-8")

    idle_meta, idle_chunks = build_chunks(idle_frames)
    sleep_meta, sleep_chunks = build_chunks(sleep_frames)
    eat_meta, eat_chunks = build_chunks(eat_frames)

    cpp = ['#include "HerculesLarvaSprites.h"', "", "namespace HerculesLarvaSprites {", ""]
    cpp.append("const RleFrame IDLE_FRAMES[] PROGMEM = {")
    for off, length, w, hgt in idle_meta:
        cpp.append(f"    {{ {off}, {length}, {w}, {hgt} }},")
    cpp.append("};")
    cpp.append("")
    cpp.append("const uint16_t IDLE_RLE[] PROGMEM = {")
    cpp.append(fmt(idle_chunks))
    cpp.append("};")
    cpp.append("")
    cpp.append("const RleFrame SLEEP_FRAMES[] PROGMEM = {")
    for off, length, w, hgt in sleep_meta:
        cpp.append(f"    {{ {off}, {length}, {w}, {hgt} }},")
    cpp.append("};")
    cpp.append("")
    cpp.append("const uint16_t SLEEP_RLE[] PROGMEM = {")
    cpp.append(fmt(sleep_chunks))
    cpp.append("};")
    cpp.append("")
    cpp.append("const RleFrame EAT_FRAMES[] PROGMEM = {")
    for off, length, w, hgt in eat_meta:
        cpp.append(f"    {{ {off}, {length}, {w}, {hgt} }},")
    cpp.append("};")
    cpp.append("")
    cpp.append("const uint16_t EAT_RLE[] PROGMEM = {")
    cpp.append(fmt(eat_chunks))
    cpp.append("};")
    cpp.append("")
    cpp.append("}")
    cpp.append("")
    OUT_CPP.write_text("\n".join(cpp), encoding="utf-8")

    (GENERATED / OUT_H.name).write_text(OUT_H.read_text(encoding="utf-8"), encoding="utf-8")
    (GENERATED / OUT_CPP.name).write_text(OUT_CPP.read_text(encoding="utf-8"), encoding="utf-8")


def main():
    idle_frames, sleep_frames, eat_frames = make_frames()
    write_preview(idle_frames, sleep_frames, eat_frames)
    write_outputs(idle_frames, sleep_frames, eat_frames)


if __name__ == "__main__":
    main()
