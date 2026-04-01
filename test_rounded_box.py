#!/usr/bin/env python3
"""
Render filled rounded boxes using exoterm's PUA corner-fill glyphs.

Full-size arcs: U+E200-U+E203
Half-size arcs: U+E204-U+E207 (for use with half-height blocks)

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
TL = "\uE200"   # top-left  corner fill  (inner = lower-right sector) - full size
TR = "\uE201"   # top-right corner fill  (inner = lower-left  sector) - full size
BL = "\uE202"   # bot-left  corner fill  (inner = upper-right sector) - full size
BR = "\uE203"   # bot-right corner fill  (inner = upper-left  sector) - full size

# Half-size arcs for use with half-height blocks
TL_half = "\uE204"   # top-left  corner fill  (inner = lower-right sector) - half size
TR_half = "\uE205"   # top-right corner fill  (inner = lower-left  sector) - half size
BL_half = "\uE206"   # bot-left  corner fill  (inner = upper-right sector) - half size
BR_half = "\uE207"   # bot-right corner fill  (inner = upper-left  sector) - half size

# Half-block characters
UPPER_HALF = "\u2580"  # ▀
LOWER_HALF = "\u2584"  # ▄

def draw_box(row, col, w, h, box_color, outer_color, label=""):
    """
    Draw a filled rounded box using full-size arcs.
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


def draw_halfsize_box(row, col, w, h_half, box_color, outer_color, label=""):
    """
    Draw a filled rounded box using half-size arcs and half-height blocks.
      w, h_half   — width and height in "half-rows" (actual rows = h_half * 0.5)
      box_color   — 256-color index for the box interior
      outer_color — 256-color index for the surrounding background

    Each logical row is drawn as two physical rows using upper/lower half blocks.
    """
    buf = []

    # Each logical "half-row" corresponds to 0.5 physical rows
    # We need to draw h_half * 2 physical rows
    actual_height = h_half * 2

    for r in range(actual_height):
        buf.append(cup(row + r, col))
        logical_row = r // 2
        is_upper_half = (r % 2 == 0)

        if logical_row == 0:
            # # Top row: upper half uses TL_half, lower half is empty
            if is_upper_half:
                buf.append(fg256(box_color) + bg256(outer_color) + TL_half)
                buf.append(bg256(box_color) + fg256(outer_color) + UPPER_HALF * (w - 2))
                buf.append(fg256(box_color) + bg256(outer_color) + TR_half)
            else:
                # Lower half of top row: just the box color
                buf.append(bg256(box_color) + fg256(outer_color) + " " * w)

        elif logical_row == h_half - 1:
            # Bottom row: upper half is box color, lower half uses BL_half/BR_half
            if is_upper_half:
                buf.append(bg256(box_color) + fg256(outer_color) + " " * w)
            else:
                buf.append(fg256(box_color) + bg256(outer_color) + BL_half)
                buf.append(bg256(box_color) + fg256(outer_color) + LOWER_HALF * (w - 2))
                buf.append(fg256(box_color) + bg256(outer_color) + BR_half)

        else:
            # Interior rows
            if is_upper_half:
                # Upper half: box color
                buf.append(bg256(box_color) + fg256(outer_color) + " " * w)
            else:
                # Lower half: also box color (creates solid interior)
                if logical_row == h_half // 2 and label:
                    pad = (w - len(label)) // 2
                    buf.append(bg256(box_color) + fg256(outer_color) + " " * pad + label + " " * (w - pad - len(label)))
                else:
                    buf.append(bg256(box_color) + fg256(outer_color) + " " * w)

    buf.append(reset())
    return "".join(buf)


def draw_box_with_half_blocks(row, col, w, h, box_color, outer_color, label=""):
    """
    Draw a box using half-size arcs ACTUALLY combined with half-block characters.
    This shows how to properly use half-size arcs with half-blocks.

    h is in full rows, but we use half-blocks to get finer vertical resolution.
    """
    buf = []

    for r in range(h * 2):  # Double the vertical resolution
        buf.append(cup(row + r, col))
        logical_row = r // 2
        is_upper_half = (r % 2 == 0)

        if r == 0:
            # Very top: use upper half-block with half-size top-left arc
            buf.append(fg256(box_color) + bg256(outer_color) + TL_half)
            buf.append(bg256(box_color) + fg256(outer_color) + UPPER_HALF * (w - 2))
            buf.append(fg256(box_color) + bg256(outer_color) + TR_half)
        elif r == h * 2 - 1:
            # Very bottom: use lower half-block with half-size bottom-right arc
            buf.append(fg256(box_color) + bg256(outer_color) + BL_half)
            buf.append(bg256(box_color) + fg256(outer_color) + LOWER_HALF * (w - 2))
            buf.append(fg256(box_color) + bg256(outer_color) + BR_half)
        else:
            # Interior: use appropriate half-blocks
            if is_upper_half:
                # Upper half of a row
                if logical_row == h // 2 and label and r == h:  # Middle row
                    pad = (w - len(label)) // 2
                    buf.append(bg256(box_color) + fg256(outer_color) +
                               UPPER_HALF * pad + label + UPPER_HALF * (w - pad - len(label)))
                else:
                    buf.append(bg256(box_color) + fg256(outer_color) + UPPER_HALF * w)
            else:
                # Lower half of a row
                buf.append(bg256(box_color) + fg256(outer_color) + LOWER_HALF * w)

    buf.append(reset())
    return "".join(buf)


out = []
out.append(clear())
out.append(cup(1, 1))

# # Box 1 — steel blue on default background (full-size arcs)
# out.append(draw_box(2, 4, 32, 7, box_color=24, outer_color=0,
#                     label="full-size arcs"))

# # Box 2 — dark red on default background (full-size arcs)
# out.append(draw_box(11, 4, 32, 7, box_color=88, outer_color=0,
#                     label="another box"))

# # Box 3 — nested: green box on blue background (demonstrates corners blend)
# out.append(bg256(24) + "".join(
#     cup(20 + r, 4) + " " * 40 for r in range(9)
# ))
# out.append(draw_box(21, 6, 28, 7, box_color=22, outer_color=24,
#                     label="nested box"))

# Box 4 — magenta using half-size arcs (positioned below box 3)
out.append(draw_halfsize_box(2, 4, 32, 5, box_color=127, outer_color=0,
                             label="half-size arcs"))

# Box 5 — cyan using half-size arcs, nested on magenta background
out.append(bg256(127) + "".join(
    cup(13 + r, 4) + " " * 32 for r in range(10)
))
out.append(draw_halfsize_box(14, 6, 28, 4, box_color=51, outer_color=127,
                             label="nested half"))

out.append(reset())
out.append(cup(50, 1))

sys.stdout.write("".join(out))
sys.stdout.flush()
