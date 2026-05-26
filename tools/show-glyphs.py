#!/usr/bin/env python3
"""Print all exoterm built-in line-drawing glyphs with code points and names."""

import unicodedata
import sys

# Glyph ranges covered by built-in tables
# (start, end, label)
RANGES = [
    (0x2500, 0x259f, "Box Drawing & Block Elements"),
    (0x25a0, 0x25b1, "Geometric Shapes (square/rectangle)"),
    (0x25c0, 0x25d8, "Geometric Shapes (triangle/circle)"),
    (0x2b00, 0x2b24, "Miscellaneous Symbols"),
    (0x23a0, 0x23ce, "Dentistry / Bracket"),
    (0x23f4, 0x23fa, "Media Control"),
]

# Descriptive overrides for glyphs whose unicodedata name is not helpful
DESC = {
    # 25a0 table
    0x25a0: "BLACK SQUARE",
    0x25a1: "WHITE SQUARE",
    0x25a2: "WHITE SQUARE WITH ROUNDED CORNERS",
    0x25a3: "WHITE SQUARE CONTAINING BLACK SMALL SQUARE",
    0x25a4: "SQUARE WITH HORIZONTAL FILL",
    0x25a5: "SQUARE WITH VERTICAL FILL",
    0x25a6: "SQUARE WITH ORTHOGONAL CROSSHATCH FILL",
    0x25a7: "SQUARE WITH UPPER LEFT TO LOWER RIGHT FILL",
    0x25a8: "SQUARE WITH UPPER RIGHT TO LOWER LEFT FILL",
    0x25a9: "SQUARE WITH DIAGONAL CROSSHATCH FILL",
    0x25aa: "BLACK SMALL SQUARE",
    0x25ab: "WHITE SMALL SQUARE",
    0x25ac: "BLACK RECTANGLE",
    0x25ad: "WHITE RECTANGLE",
    0x25ae: "BLACK VERTICAL RECTANGLE",
    0x25af: "WHITE VERTICAL RECTANGLE",
    0x25b0: "BLACK HORIZONTAL RECTANGLE",
    0x25b1: "WHITE HORIZONTAL RECTANGLE",
    # 25c0 table
    0x25c9: "FISHEYE",
    0x25cb: "WHITE CIRCLE",
    0x25cf: "BLACK CIRCLE",
    0x25d8: "INVERSE BULLET (half-size filled)",
    # 2b00 table
    0x2b12: "SQUARE WITH TOP HALF BLACK (bordered)",
    0x2b13: "SQUARE WITH BOTTOM HALF BLACK (bordered)",
    0x2b1a: "DOTTED SQUARE",
    0x2b1b: "BLACK LARGE SQUARE",
    0x2b1c: "WHITE LARGE SQUARE",
    0x2b1d: "BLACK VERY SMALL SQUARE",
    0x2b1e: "WHITE VERY SMALL SQUARE",
    0x2b24: "BLACK LARGE CIRCLE",
    # 23a0 table
    0x23a1: "LEFT SQUARE BRACKET UPPER CORNER",
    0x23a2: "LEFT SQUARE BRACKET EXTENSION",
    0x23a3: "LEFT SQUARE BRACKET LOWER CORNER",
    0x23a4: "RIGHT BRACKET UPPER CORNER",
    0x23a5: "RIGHT BRACKET EXTENSION",
    0x23a6: "RIGHT BRACKET LOWER CORNER",
    0x23af: "HORIZONTAL LINE EXTENSION",
    0x23bf: "DENTISTRY SYMBOL LIGHT VERTICAL AND BOTTOM RIGHT",
    # 23f4 table
    0x23f4: "BLACK MEDIUM LEFT-POINTING TRIANGLE",
    0x23f5: "BLACK MEDIUM RIGHT-POINTING TRIANGLE",
    0x23f6: "BLACK MEDIUM UP-POINTING TRIANGLE",
    0x23f7: "BLACK MEDIUM DOWN-POINTING TRIANGLE",
    0x23f8: "DOUBLE VERTICAL BAR",
    0x23f9: "BLACK MEDIUM SQUARE",
    0x23fa: "BLACK MEDIUM CIRCLE",
}


def glyph_name(cp):
    if cp in DESC:
        return DESC[cp]
    try:
        return unicodedata.name(chr(cp))
    except ValueError:
        return f"<unknown>"


def print_row(cp):
    name = glyph_name(cp)
    if cp == 0x00a0:
        # nbsp
        sys.stdout.write(f"U+{cp:04X}  {name}\n")
    else:
        sys.stdout.write(f"U+{cp:04X}  {chr(cp)}  {name}\n")


def print_range(start, end):
    for cp in range(start, end + 1):
        print_row(cp)


def main():
    for start, end, label in RANGES:
        print(f"\n{'═' * 70}")
        print(f"  {label}  (U+{start:04X}–U+{end:04X})")
        print(f"{'─' * 70}")
        print_range(start, end)

    # PUA rounded corners
    print(f"\n{'═' * 70}")
    print(f"  Rounded Corner Fills (PUA)")
    print(f"{'─' * 70}")
    pua_names = [
        (0xe200, "TOP-LEFT CORNER FILL (full size)"),
        (0xe201, "TOP-RIGHT CORNER FILL (full size)"),
        (0xe202, "BOT-LEFT CORNER FILL (full size)"),
        (0xe203, "BOT-RIGHT CORNER FILL (full size)"),
        (0xe204, "TOP-LEFT CORNER FILL (half size)"),
        (0xe205, "TOP-RIGHT CORNER FILL (half size)"),
        (0xe206, "BOT-LEFT CORNER FILL (half size)"),
        (0xe207, "BOT-RIGHT CORNER FILL (half size)"),
    ]
    for cp, name in pua_names:
        sys.stdout.write(f"U+{cp:04X}  {chr(cp)}  {name}\n")

    print()


if __name__ == "__main__":
    main()
