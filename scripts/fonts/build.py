#!/usr/bin/env python3
"""Build the two computerscare fonts; no font tooling is needed by the plugin.

Four deterministic outlines per character, shared advance widths, and private
Unicode aliases for NanoVG (which does not apply OpenType stylistic alternates).
The ordinary Unicode mapping remains usable outside Rack. SS02–SS04 expose the
three additional alternates in applications with OpenType support.
"""
import argparse
import copy
import hashlib
import importlib.util
import json
import random
from pathlib import Path

from fontTools.feaLib.builder import addOpenTypeFeaturesFromString
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.recordingPen import DecomposingRecordingPen
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTFont
from fontTools.ttLib.tables._g_l_y_f import GlyphCoordinates

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "res/fonts/computerscare"
spec = importlib.util.spec_from_file_location("distort", ROOT / "scripts/svg_text_randomize.py")
distort = importlib.util.module_from_spec(spec)
spec.loader.exec_module(distort)
PRIVATE = 0xE000
COUNT = 4


def seed(text):
    return int.from_bytes(hashlib.sha256(text.encode()).digest()[:8], "little")


def outline(source, name):
    record = DecomposingRecordingPen(source.getGlyphSet())
    source.getGlyphSet()[name].draw(record)
    pen = TTGlyphPen(None)
    record.replay(pen)
    return pen.glyph()


def capital_q(source, width):
    # Barlow's Q has a central vertical stem, easily mistaken for O at small
    # sizes. Use its O bowl with a conventional, broad diagonal tail instead.
    name = source.getBestCmap()[ord("O")]
    pen = TTGlyphPen(None)
    source.getGlyphSet()[name].draw(
        TransformPen(pen, (width / source["hmtx"].metrics[name][0], 0, 0, 1, 0, 0)))
    cap = source["OS/2"].sCapHeight
    pen.moveTo((width * .51, cap * .20))
    pen.lineTo((width * .64, cap * .23))
    pen.lineTo((width * .91, -cap * .07))
    pen.lineTo((width * .76, -cap * .10))
    pen.closePath()
    return pen.glyph()


def mutation_profile(config, cp):
    amount = config.get("glyph_distortion", {}).get(chr(cp), config["distortion"])
    profile = distort.mutation_profile(amount)
    profile["rotation"] *= config["rotation_scale"]
    profile["translate_y"] *= .55
    profile["spacing"] *= .3
    profile["drop_chance"] = 0
    profile["transform_chance"] = 1
    return profile


def mutate(glyph, width, upm, profile, key):
    glyph = copy.deepcopy(glyph)
    if not glyph.numberOfContours:
        return glyph
    scale = 72 / upm
    segments = []
    start = 0
    for end in glyph.endPtsOfContours:
        for i in range(start, end + 1):
            x, y = glyph.coordinates[i]
            segments.append({"cmd": "M" if i == start else "L", "points": [[x * scale, y * scale]], "raw": []})
        segments.append({"cmd": "Z", "points": [], "raw": []})
        start = end + 1
    distort.randomize_segments(segments, profile, random.Random(key))
    points = [p for segment in segments for p in segment["points"]]
    lo = min(p[0] for p in points)
    hi = max(p[0] for p in points)
    # A fixed advance across all four alternates prevents the word layout from
    # changing with its seed. Keep ink inside that advance, including swashes.
    margin = min(width * .08, 22) * scale
    available = max(1, width * scale - 2 * margin)
    squeeze = min(1, available / max(.01, hi - lo)) if width else 1
    move = max(margin - lo * squeeze, min(0, width * scale - margin - hi * squeeze)) if width else 0
    glyph.coordinates = GlyphCoordinates([(round((x * squeeze + move) / scale), round(y / scale)) for x, y in points])
    return glyph


def build(config, codepoints):
    source_path = Path(__file__).parent / "sources" / config["source"]
    source = TTFont(source_path)
    cmap = source.getBestCmap()
    upm = source["head"].unitsPerEm
    font = FontBuilder(upm, isTTF=True)
    glyphs = {".notdef": outline(source, ".notdef")}
    metrics = {".notdef": source["hmtx"].metrics[".notdef"]}
    mapping, features = {}, [[], [], [], []]
    for index, cp in enumerate(codepoints):
        original_name = cmap[cp]
        original = outline(source, original_name)
        width = source["hmtx"].metrics[original_name][0]
        if cp == ord("Q"):
            original = capital_q(source, width)
        profile = mutation_profile(config, cp)
        for variant in range(COUNT):
            name = f"u{cp:04X}.v{variant}"
            shape = mutate(original, width, upm, profile, seed(f"{config['family']}:{cp}:{variant}"))
            glyphs[name] = shape
            shape.recalcBounds(None)
            metrics[name] = (width, getattr(shape, "xMin", 0))
            mapping[PRIVATE + index * COUNT + variant] = name
            if variant == 0:
                mapping[cp] = name
            else:
                features[variant].append(f"sub u{cp:04X}.v0 by {name};")
    font.setupGlyphOrder(list(glyphs))
    font.setupCharacterMap(mapping)
    font.setupGlyf(glyphs)
    font.setupHorizontalMetrics(metrics)
    font.setupHorizontalHeader(ascent=880, descent=-220, lineGap=0)
    copyright_text = "Copyright 2017 The Barlow Project Authors (https://github.com/jpt/barlow). Modified 2026 Adam Malone."
    font.setupNameTable({
        "familyName": config["family"], "styleName": "Regular",
        "uniqueFontIdentifier": config["family"] + " 1.000",
        "fullName": config["family"], "psName": config["family"].replace(" ", ""),
        "version": "Version 1.000", "copyright": copyright_text,
        "licenseDescription": "SIL Open Font License, Version 1.1. See OFL.txt.",
        "licenseInfoURL": "https://openfontlicense.org/",
    })
    font.setupOS2(sTypoAscender=880, sTypoDescender=-220, sTypoLineGap=0,
                  usWinAscent=max(getattr(g, "yMax", 0) for g in glyphs.values()) + 20,
                  usWinDescent=-min(getattr(g, "yMin", 0) for g in glyphs.values()) + 20,
                  sxHeight=source["OS/2"].sxHeight, sCapHeight=source["OS/2"].sCapHeight,
                  usWeightClass=config["weight"])
    font.setupPost()
    font.setupMaxp()
    addOpenTypeFeaturesFromString(font.font, "\n".join(
        f"feature ss{variant + 1:02} {{ {' '.join(features[variant])} }} ss{variant + 1:02};" for variant in range(1, COUNT)))
    # Reproducible files, independent of filesystem dates / regeneration time.
    font.font["head"].created = font.font["head"].modified = 3878102400
    font.font.recalcTimestamp = False
    path = OUT / config["file"]
    font.save(path)
    return {"file": config["file"], "source": config["source"],
            "source_sha256": hashlib.sha256(source_path.read_bytes()).hexdigest(),
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "characters": len(codepoints), "variants": COUNT, "glyphs": len(glyphs)}


def validate():
    manifest = json.loads((OUT / "manifest.json").read_text())
    for item in manifest:
        path = OUT / item["file"]
        assert hashlib.sha256(path.read_bytes()).hexdigest() == item["sha256"]
        font = TTFont(path)
        cmap = font.getBestCmap()
        original = sorted(cp for cp in cmap if not PRIVATE <= cp < 0xF900)
        assert len(original) == item["characters"]
        for index, cp in enumerate(original):
            names = [cmap[PRIVATE + index * COUNT + v] for v in range(COUNT)]
            assert len(set(names)) == COUNT
            advances = [font["hmtx"].metrics[name][0] for name in names]
            assert len(set(advances)) == 1
            shapes = [font["glyf"][name] for name in names]
            for shape in shapes:
                if shape.numberOfContours and advances[0]:
                    assert shape.xMin >= 0 and shape.xMax <= advances[0]
            if chr(cp).isalnum():
                assert len({tuple(shape.coordinates) for shape in shapes}) == COUNT, hex(cp)
        print(f"{item['file']}: {len(original)} characters × four distinct outlines; shared advances, bounds and checksum passed")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    if args.check:
        validate()
        return
    config = json.loads((Path(__file__).parent / "profiles.json").read_text())
    maps = [TTFont(Path(__file__).parent / "sources" / c["source"]).getBestCmap() for c in config]
    codepoints = sorted(cp for cp in set.intersection(*(set(m) for m in maps)) if cp >= 32 and not 0xE000 <= cp < 0xF900)
    assert PRIVATE + len(codepoints) * COUNT < 0xF900
    OUT.mkdir(exist_ok=True)
    (OUT / "manifest.json").write_text(json.dumps([build(c, codepoints) for c in config], indent=2) + "\n")
    lines = ["// Generated by scripts/fonts/build.py. Do not edit.", "#pragma once", "#include <cstdint>",
             "namespace computerscare { namespace type {", "constexpr uint32_t glyphCodepoints[] = {"]
    for pos in range(0, len(codepoints), 12):
        lines.append("    " + ", ".join(f"0x{cp:04x}" for cp in codepoints[pos:pos + 12]) + ",")
    lines += ["};", "constexpr uint32_t glyphPrivateBase = 0xe000;", "constexpr unsigned glyphVariants = 4;", "}}"]
    (ROOT / "src/ComputerscareFontGlyphs.hpp").write_text("\n".join(lines) + "\n")
    validate()


if __name__ == "__main__":
    main()
