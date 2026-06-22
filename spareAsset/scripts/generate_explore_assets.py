#!/usr/bin/env python3
from pathlib import Path
import re

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
SRC_PARK = ROOT / "spareAsset/origin/explore/park2"
SRC_BACK_HILL = ROOT / "spareAsset/origin/explore/backHill"
SRC_RIVERSIDE = ROOT / "spareAsset/origin/explore/reiverside"
SRC_OLD_WOODS = ROOT / "spareAsset/origin/explore/oldWoods2"
OUT_H = ROOT / "src/assets/ExploreAssets.h"
OUT_CPP = ROOT / "src/assets/ExploreAssets.cpp"
PREVIEWS = ROOT / "spareAsset/previews/explore"
GENERATED = ROOT / "spareAsset/generated/src_assets"

FRAME_W = 240
FRAME_H = 135


def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def image_order(path):
    match = re.search(r"\((\d+)\)", path.name)
    return int(match.group(1)) if match else 999


def fit_fullscreen(img):
    img = img.convert("RGB")
    src_ratio = img.width / img.height
    dst_ratio = FRAME_W / FRAME_H
    if src_ratio > dst_ratio:
        new_w = round(img.height * dst_ratio)
        left = (img.width - new_w) // 2
        img = img.crop((left, 0, left + new_w, img.height))
    elif src_ratio < dst_ratio:
        new_h = round(img.width / dst_ratio)
        top = (img.height - new_h) // 2
        img = img.crop((0, top, img.width, top + new_h))
    return img.resize((FRAME_W, FRAME_H), Image.Resampling.LANCZOS)


def encode_indexed(img):
    q = img.quantize(colors=256, method=Image.Quantize.MEDIANCUT)
    palette_raw = q.getpalette()[: 256 * 3]
    palette = []
    for i in range(256):
        base = i * 3
        palette.append(rgb565(palette_raw[base], palette_raw[base + 1], palette_raw[base + 2]))
    return list(q.getdata()), palette, q.convert("RGB")


def fmt_u8(vals):
    return "\n".join(
        "    " + ", ".join(f"0x{v:02X}" for v in vals[i : i + 16]) + ","
        for i in range(0, len(vals), 16)
    )


def fmt_u16(vals):
    return "\n".join(
        "    " + ", ".join(f"0x{v:04X}" for v in vals[i : i + 12]) + ","
        for i in range(0, len(vals), 12)
    )


def load_park_frames():
    files = sorted(SRC_PARK.glob("*.png"), key=image_order)
    if len(files) < 3:
        raise ValueError(f"Expected 3 park backgrounds in {SRC_PARK}, got {len(files)}")
    frames = []
    for idx, src in enumerate(files[:3]):
        frame = fit_fullscreen(Image.open(src))
        frames.append(frame)
        frame.save(PREVIEWS / f"park_{idx}.png")
        print(f"park_{idx}: {src.name} -> {FRAME_W}x{FRAME_H}")
    sheet = Image.new("RGB", (FRAME_W * 3, FRAME_H))
    for idx, frame in enumerate(frames):
        sheet.paste(frame, (idx * FRAME_W, 0))
    sheet.save(PREVIEWS / "park_sheet_preview.png")
    return frames


def load_single_frame(src_dir, label):
    files = sorted(src_dir.glob("*.png"), key=image_order)
    if not files:
        raise ValueError(f"Expected at least 1 {label} background in {src_dir}")
    frame = fit_fullscreen(Image.open(files[0]))
    frame.save(PREVIEWS / f"{label}.png")
    print(f"{label}: {files[0].name} -> {FRAME_W}x{FRAME_H}")
    return frame


def load_day_night_frames(src_dir, label):
    files = sorted(src_dir.glob("*.png"), key=image_order)
    if len(files) < 2:
        raise ValueError(f"Expected 2 {label} backgrounds in {src_dir}, got {len(files)}")
    frames = []
    for idx, src in enumerate(files[:2]):
        frame = fit_fullscreen(Image.open(src))
        frames.append(frame)
        frame.save(PREVIEWS / f"{label}_{idx}.png")
        print(f"{label}_{idx}: {src.name} -> {FRAME_W}x{FRAME_H}")
    sheet = Image.new("RGB", (FRAME_W * 2, FRAME_H))
    for idx, frame in enumerate(frames):
        sheet.paste(frame, (idx * FRAME_W, 0))
    sheet.save(PREVIEWS / f"{label}_sheet_preview.png")
    return frames


def save_all_sheet(named_frames):
    sheet = Image.new("RGB", (FRAME_W * len(named_frames), FRAME_H))
    for idx, (_, frame) in enumerate(named_frames):
        sheet.paste(frame, (idx * FRAME_W, 0))
    sheet.save(PREVIEWS / "explore_backgrounds_preview.png")


def write_outputs(park_frames, back_hill_frame, riverside_frames, old_woods_frames):
    OUT_H.write_text(
        f"""#pragma once
#include <Arduino.h>
#include <cstdint>

namespace ExploreAssets {{

static constexpr int FRAME_W = {FRAME_W};
static constexpr int FRAME_H = {FRAME_H};
static constexpr uint8_t PARK_BG_COUNT = {len(park_frames)};
static constexpr uint8_t RIVERSIDE_BG_COUNT = {len(riverside_frames)};
static constexpr uint8_t OLD_WOODS_BG_COUNT = {len(old_woods_frames)};

struct IndexedImage {{
    const uint8_t* indices;
    const uint16_t* palette;
}};

extern const uint8_t PARK_MORNING_INDEX[] PROGMEM;
extern const uint16_t PARK_MORNING_PALETTE[] PROGMEM;
extern const uint8_t PARK_AFTERNOON_INDEX[] PROGMEM;
extern const uint16_t PARK_AFTERNOON_PALETTE[] PROGMEM;
extern const uint8_t PARK_EVENING_INDEX[] PROGMEM;
extern const uint16_t PARK_EVENING_PALETTE[] PROGMEM;
extern const uint8_t BACK_HILL_DAY_INDEX[] PROGMEM;
extern const uint16_t BACK_HILL_DAY_PALETTE[] PROGMEM;
extern const uint8_t RIVERSIDE_DAY_INDEX[] PROGMEM;
extern const uint16_t RIVERSIDE_DAY_PALETTE[] PROGMEM;
extern const uint8_t RIVERSIDE_NIGHT_INDEX[] PROGMEM;
extern const uint16_t RIVERSIDE_NIGHT_PALETTE[] PROGMEM;
extern const uint8_t OLD_WOODS_DAY_INDEX[] PROGMEM;
extern const uint16_t OLD_WOODS_DAY_PALETTE[] PROGMEM;
extern const uint8_t OLD_WOODS_NIGHT_INDEX[] PROGMEM;
extern const uint16_t OLD_WOODS_NIGHT_PALETTE[] PROGMEM;

IndexedImage parkBackground(uint8_t timeOfDay);
IndexedImage background(uint8_t location, uint8_t timeOfDay);

}}
""",
        encoding="utf-8",
    )

    entries = [
        ("PARK_MORNING", park_frames[0]),
        ("PARK_AFTERNOON", park_frames[1]),
        ("PARK_EVENING", park_frames[2]),
        ("BACK_HILL_DAY", back_hill_frame),
        ("RIVERSIDE_DAY", riverside_frames[0]),
        ("RIVERSIDE_NIGHT", riverside_frames[1]),
        ("OLD_WOODS_DAY", old_woods_frames[0]),
        ("OLD_WOODS_NIGHT", old_woods_frames[1]),
    ]
    cpp = ['#include "ExploreAssets.h"', "", "namespace ExploreAssets {", ""]
    for name, frame in entries:
        indices, palette, preview = encode_indexed(frame)
        preview.save(PREVIEWS / f"{name.lower()}_indexed_preview.png")
        cpp.append(f"const uint8_t {name}_INDEX[] PROGMEM = {{")
        cpp.append(fmt_u8(indices))
        cpp.append("};")
        cpp.append("")
        cpp.append(f"const uint16_t {name}_PALETTE[] PROGMEM = {{")
        cpp.append(fmt_u16(palette))
        cpp.append("};")
        cpp.append("")
    cpp.append("IndexedImage parkBackground(uint8_t timeOfDay) {")
    cpp.append("    switch (timeOfDay) {")
    cpp.append("        case 1: return { PARK_AFTERNOON_INDEX, PARK_AFTERNOON_PALETTE };")
    cpp.append("        case 2: return { PARK_EVENING_INDEX, PARK_EVENING_PALETTE };")
    cpp.append("        case 0:")
    cpp.append("        default: return { PARK_MORNING_INDEX, PARK_MORNING_PALETTE };")
    cpp.append("    }")
    cpp.append("}")
    cpp.append("")
    cpp.append("IndexedImage background(uint8_t location, uint8_t timeOfDay) {")
    cpp.append("    switch (location) {")
    cpp.append("        case 0: return parkBackground(timeOfDay);")
    cpp.append("        case 1: return { BACK_HILL_DAY_INDEX, BACK_HILL_DAY_PALETTE };")
    cpp.append("        case 2: return timeOfDay == 2")
    cpp.append("            ? IndexedImage{ RIVERSIDE_NIGHT_INDEX, RIVERSIDE_NIGHT_PALETTE }")
    cpp.append("            : IndexedImage{ RIVERSIDE_DAY_INDEX, RIVERSIDE_DAY_PALETTE };")
    cpp.append("        case 3: return timeOfDay == 2")
    cpp.append("            ? IndexedImage{ OLD_WOODS_NIGHT_INDEX, OLD_WOODS_NIGHT_PALETTE }")
    cpp.append("            : IndexedImage{ OLD_WOODS_DAY_INDEX, OLD_WOODS_DAY_PALETTE };")
    cpp.append("        default: return { nullptr, nullptr };")
    cpp.append("    }")
    cpp.append("}")
    cpp.append("")
    cpp.append("}")
    cpp.append("")
    OUT_CPP.write_text("\n".join(cpp), encoding="utf-8")


def main():
    PREVIEWS.mkdir(parents=True, exist_ok=True)
    GENERATED.mkdir(parents=True, exist_ok=True)
    park_frames = load_park_frames()
    back_hill_frame = load_single_frame(SRC_BACK_HILL, "back_hill")
    riverside_frames = load_day_night_frames(SRC_RIVERSIDE, "riverside")
    old_woods_frames = load_day_night_frames(SRC_OLD_WOODS, "old_woods")
    save_all_sheet([
        ("park_morning", park_frames[0]),
        ("park_afternoon", park_frames[1]),
        ("park_evening", park_frames[2]),
        ("back_hill_day", back_hill_frame),
        ("riverside_day", riverside_frames[0]),
        ("riverside_night", riverside_frames[1]),
        ("old_woods_day", old_woods_frames[0]),
        ("old_woods_night", old_woods_frames[1]),
    ])
    write_outputs(park_frames, back_hill_frame, riverside_frames, old_woods_frames)
    (GENERATED / OUT_H.name).write_text(OUT_H.read_text(encoding="utf-8"), encoding="utf-8")
    (GENERATED / OUT_CPP.name).write_text(OUT_CPP.read_text(encoding="utf-8"), encoding="utf-8")


if __name__ == "__main__":
    main()
