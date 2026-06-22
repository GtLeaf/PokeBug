#!/usr/bin/env python3
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
OUT_H = ROOT / "src/assets/MainSceneAssets.h"
OUT_CPP = ROOT / "src/assets/MainSceneAssets.cpp"
PREVIEWS = ROOT / "spareAsset/previews/mainScene"
GENERATED = ROOT / "spareAsset/generated/src_assets"

SOURCES = [
    ("MOSS_BG", ROOT / "asset/mainScene/box/moss/bg_moss.png", 200, 120),
    ("MOSS_GROUND", ROOT / "asset/mainScene/box/moss/ground_moss.png", 200, 15),
    ("MOSS_STATE", ROOT / "asset/mainScene/box/moss/state_moss.png", 40, 135),
    ("BEGINNER_FULL", ROOT / "asset/mainScene/box/beginnerBox/bg_full_beginner.png", 240, 135),
    ("CHILD_ROOM_FULL", ROOT / "asset/mainScene/box/childRoom/child_room.png", 240, 135),
]


def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def fit_exact(path, w, h):
    return Image.open(path).convert("RGB").resize((w, h), Image.Resampling.LANCZOS)


def quantize_indexed(img):
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


def write_outputs(frames):
    h = [
        "#pragma once",
        "#include <Arduino.h>",
        "#include <cstdint>",
        "",
        "namespace MainSceneAssets {",
        "",
    ]
    for name, w, hgt, _, _ in frames:
        h.append(f"extern const int {name}_W;")
        h.append(f"extern const int {name}_H;")
        h.append(f"extern const uint8_t {name}_INDEX[] PROGMEM;")
        h.append(f"extern const uint16_t {name}_PALETTE[] PROGMEM;")
        h.append("")
    h.append("}")
    h.append("")
    OUT_H.write_text("\n".join(h), encoding="utf-8")

    cpp = ['#include "MainSceneAssets.h"', "", "namespace MainSceneAssets {", ""]
    for name, w, hgt, indices, palette in frames:
        cpp.append(f"const int {name}_W = {w};")
        cpp.append(f"const int {name}_H = {hgt};")
        cpp.append(f"const uint8_t {name}_INDEX[] PROGMEM = {{")
        cpp.append(fmt_u8(indices))
        cpp.append("};")
        cpp.append("")
        cpp.append(f"const uint16_t {name}_PALETTE[] PROGMEM = {{")
        cpp.append(fmt_u16(palette))
        cpp.append("};")
        cpp.append("")
    cpp.append("}")
    cpp.append("")
    OUT_CPP.write_text("\n".join(cpp), encoding="utf-8")


def main():
    PREVIEWS.mkdir(parents=True, exist_ok=True)
    GENERATED.mkdir(parents=True, exist_ok=True)

    frames = []
    for name, path, w, h in SOURCES:
        img = fit_exact(path, w, h)
        indices, palette, preview = quantize_indexed(img)
        preview.save(PREVIEWS / f"{name.lower()}_indexed_preview.png")
        frames.append((name, w, h, indices, palette))
        print(f"{name}: {path.name} -> {w}x{h}, indexed8 + 256-color palette")

    write_outputs(frames)
    (GENERATED / OUT_H.name).write_text(OUT_H.read_text(encoding="utf-8"), encoding="utf-8")
    (GENERATED / OUT_CPP.name).write_text(OUT_CPP.read_text(encoding="utf-8"), encoding="utf-8")


if __name__ == "__main__":
    main()
