#!/usr/bin/env python3
"""
Render a filled rounded box using exoterm's PUA corner-fill glyphs (U+E200-U+E203).

Corner cells:  fg = box colour,  bg = outer colour  →  glyph fills inner sector
Interior/edge: fg = anything,    bg = box colour     →  space fills the whole cell
"""

import sys

ESC = "\033"

def cup(row, col):      return f"{ESC}[{row};{col}H"
def fg256(n):           return f"{ESC}[38;5;{n}m"
def bg256(n):           return f"{ESC}[48;5;{n}m"
def reset():            return f"{ESC}[0m"
def clear():            return f"{ESC}[2J"

# PUA glyphs defined in linedraw_e200.h
TL = "\uE200"   # top-left  corner fill  (inner = lower-right sector)
TR = "\uE201"   # top-right corner fill  (inner = lower-left  sector)
BL = "\uE202"   # bot-left  corner fill  (inner = upper-right sector)
BR = "\uE203"   # bot-right corner fill  (inner = upper-left  sector)

def draw_box(row, col, w, h, box_color, outer_color, label=""):
    """
    Draw a filled rounded box.
      w, h        — total size including corner cells
      box_color   — 256-color index for the box interior
      outer_color — 256-color index for the surrounding background
    """
    buf = []

    corner_prefix = fg256(box_color) + bg256(outer_color)
    fill           = bg256(box_color) + fg256(outer_color)

    for r in range(h):
        buf.append(cup(row + r, col))
        if r == 0:
            # top row: TL corner, top edge, TR corner
            buf.append(corner_prefix + TL)
            buf.append(fill + " " * (w - 2))
            buf.append(corner_prefix + TR)
        elif r == h - 1:
            # bottom row: BL corner, bottom edge, BR corner
            buf.append(corner_prefix + BL)
            buf.append(fill + " " * (w - 2))
            buf.append(corner_prefix + BR)
        else:
            # interior row: plain fill
            if r == h // 2 and label:
                pad = (w - len(label)) // 2
                buf.append(fill + " " * pad + label + " " * (w - pad - len(label)))
            else:
                buf.append(fill + " " * w)

    buf.append(reset())
    return "".join(buf)


out = []
out.append(clear())
out.append(cup(1, 1))

# Box 1 — steel blue on default background
out.append(draw_box(2, 4, 32, 7, box_color=24, outer_color=0,
                    label="rounded fill"))

# Box 2 — dark red on default background
out.append(draw_box(11, 4, 32, 7, box_color=88, outer_color=0,
                    label="another box"))

# Box 3 — nested: green box on blue background (demonstrates corners blend)
out.append(bg256(24) + "".join(
    cup(20 + r, 4) + " " * 40 for r in range(9)
))
out.append(draw_box(21, 6, 28, 7, box_color=22, outer_color=24,
                    label="nested box"))

out.append(reset())
out.append(cup(31, 1))

sys.stdout.write("".join(out))
sys.stdout.flush()
