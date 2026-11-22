#!/usr/bin/env python3
"""Writes assets/font/unifont_subset.hex: the GNU Unifont glyphs of every character the ports' translation tables and
their own screens use, in Unifont's .hex format (CODEPOINT:HEXBITS, 16 rows of 8 or 16 pixels), and
assets/font/unifont_subset.otf: the same characters as an outline font, for text drawn by Dear ImGui (launchers).

The ports draw translated text with these glyphs (hippodrome/text_font.h). Han characters and kana come from
unifont_jp (Japanese glyph shapes), everything else from unifont. The set is the union of every port's tables, so run
it with all of them after any table gains characters (a port's checkout of each repository):
    python3 tools/font/make_unifont_subset.py --tables ../plethron/translations/*.csv ../stadion/translation/*.csv
    (--extra <file>: strings a port draws itself that are in no table; --fonts: where unifont*.otf are)
--check only reports characters missing from the committed subset (exit 1 when any are).

Unifont is dual licensed under the GNU GPL 2+ with the GNU font embedding exception and the SIL Open Font License 1.1
(assets/font/LICENSE-unifont.txt). No ROM data goes into these files.
"""
import argparse
import csv
import os
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, "assets", "font", "unifont_subset.hex")
OUT_OTF = os.path.join(ROOT, "assets", "font", "unifont_subset.otf")
# Columns that hold no text to draw (the rest are languages: ja/jp, en, es, de, pt, ko, fr, it, kanji ...).
KEY_COLUMNS = {"id", "md5", "addr", "address", "kana", "note"}
# Text Plethron draws itself, besides its tables: the language select screen (text_lookup.cpp kNames and prompt).
EXTRA = "にほんご 日本語 English Español Deutsch Português 한국어 Français Italiano Press C◀ for AI translations ?"


def add_table_args(ap):
    ap.add_argument("--tables", nargs="*", default=[], help="translation tables (CSV) of every port")
    ap.add_argument("--extra", nargs="*", default=[], help="UTF-8 text files of strings a port draws itself")


def wanted_chars(tables=(), extra=()):
    chars = set(chr(c) for c in range(0x20, 0x7F))
    chars.update(chr(c) for c in range(0xA0, 0x100))  # Latin-1: accents for es/de/pt
    chars.update(EXTRA)
    for path in extra:
        with open(path, encoding="utf-8") as f:
            chars.update(f.read())
    for path in tables:
        with open(path, newline="", encoding="utf-8") as f:
            for row in csv.DictReader(f):
                for col, cell in row.items():
                    if col is not None and col not in KEY_COLUMNS:
                        chars.update(cell or "")
    return sorted(c for c in chars if c not in "\n\r\t")


def is_japanese(cp):
    return 0x3000 <= cp < 0x3100 or 0x4E00 <= cp < 0xA000 or 0x3400 <= cp < 0x4DC0 or 0xFF00 <= cp < 0xFFF0


def render(font, ch):
    width = int(round(font.getlength(ch)))
    if width not in (8, 16):
        return None
    img = Image.new("1", (width, 16), 0)
    draw = ImageDraw.Draw(img)
    draw.fontmode = "1"
    draw.text((0, 14), ch, font=font, fill=1, anchor="ls")  # Unifont's baseline is row 14
    if not ch.isspace() and img.getbbox() is None:
        return None
    rows = []
    for y in range(16):
        bits = 0
        for x in range(width):
            bits = (bits << 1) | (1 if img.getpixel((x, y)) else 0)
        rows.append(f"{bits:0{width // 4}X}")
    return "".join(rows)


def read_hex(path):
    glyphs = {}
    if os.path.exists(path):
        for line in open(path, encoding="ascii"):
            line = line.strip()
            if line and not line.startswith("#"):
                cp, bits = line.split(":")
                glyphs[int(cp, 16)] = bits
    return glyphs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--fonts", default="/usr/share/fonts/opentype/unifont")
    ap.add_argument("--check", action="store_true")
    add_table_args(ap)
    args = ap.parse_args()
    if not args.tables:
        ap.error("--tables: give every port's translation tables (the subset is shared)")
    chars = wanted_chars(args.tables, args.extra)
    if args.check:
        have = read_hex(OUT)
        missing = [c for c in chars if ord(c) not in have]
        print("missing:", "".join(missing) if missing else "none")
        return 1 if missing else 0
    # The subset only grows: a glyph another port's tables need stays even when this run was not given them.
    chars = sorted(set(chars) | {chr(cp) for cp in read_hex(OUT)})
    base = ImageFont.truetype(os.path.join(args.fonts, "unifont.otf"), 16)
    jp = ImageFont.truetype(os.path.join(args.fonts, "unifont_jp.otf"), 16)
    out, missing = [], []
    for ch in chars:
        cp = ord(ch)
        bits = (render(jp, ch) if is_japanese(cp) else None) or render(base, ch)
        if bits is None:
            missing.append(ch)
            continue
        out.append(f"{cp:04X}:{bits}")
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="ascii", newline="\n") as f:
        f.write("# GNU Unifont 15.1 subset (GPL-2.0+ with font embedding exception, or OFL-1.1): see LICENSE-unifont.txt\n")
        f.write("# Generated by tools/font/make_unifont_subset.py from the ports' tables; do not edit by hand.\n")
        f.write("\n".join(out) + "\n")
    # the outline version (Japanese shapes for Han and kana, like the bitmaps): Dear ImGui cannot read .hex
    from fontTools import subset
    opts = subset.Options()
    opts.name_IDs = ["*"]  # keep the copyright and licence names
    opts.notdef_outline = True
    font = subset.load_font(os.path.join(args.fonts, "unifont_jp.otf"), opts)
    sub = subset.Subsetter(opts)
    sub.populate(unicodes=[ord(c) for c in chars if ord(c) not in [ord(m) for m in missing]])
    sub.subset(font)
    subset.save_font(font, OUT_OTF, opts)
    print(f"{len(out)} glyphs -> {os.path.relpath(OUT, ROOT)}, {os.path.relpath(OUT_OTF, ROOT)}" + (f"; no glyph for {''.join(missing)!r}" if missing else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
