#!/usr/bin/env python3
"""
Render filled rounded boxes using exoterm's bold attribute to trigger rounded corners.

When U+259x quadrant characters are drawn with the bold attribute, they are 
rendered as rounded corners using filled arc commands.

The mapping (bold triggers arc drawing):
  U+259F + bold → full top-left arc
  U+2599 + bold → full top-right arc
  U+259C + bold → full bottom-left arc
  U+259B + bold → full bottom-right arc
  U+2597 + bold → half top-left arc
  U+2596 + bold → half top-right arc
  U+259D + bold → half bottom-left arc
  U+2598 + bold → half bottom-right arc

Corner cells:  fg = box colour,  bg = outer colour  →  glyph fills inner sector
Interior/edge: fg = anything,    bg = box colour     →  space fills the whole cell
"""

import sys

ESC = "\033"

def cup(row, col):      return f"{ESC}[{row};{col}H"
def fg256(n):           return f"{ESC}[38;5;{n}m"
def bg256(n):           return f"{ESC}[48;5;{n}m"
def bold():             return f"{ESC}[1m"
def reset():            return f"{ESC}[0m"
def clear():            return f"{ESC}[2J"

# Full block characters (with bold → rounded arcs)
FULL_TL = "\u259F"  # ▟ top-left  with bold → full arc
FULL_TR = "\u2599"  # ▙ top-right with bold → full arc
FULL_BL = "\u259C"  # ▚ bottom-left with bold → full arc
FULL_BR = "\u259B"  # ▛ bottom-right with bold → full arc

# Half block characters (with bold → rounded arcs)
HALF_TL = "\u2597"  # ▗ top-left  with bold → half arc
HALF_TR = "\u2596"  # ▖ top-right with bold → half arc
HALF_BL = "\u259D"  # ▝ bottom-left with bold → half arc
HALF_BR = "\u2598"  # ▘ bottom-right with bold → half arc

# Half-block characters for interior
UPPER_HALF = "\u2580"  # ▀
LOWER_HALF = "\u2584"  # ▄
LEFT_HALF = "\u258C"   # ▌
RIGHT_HALF = "\u2590"  # ▐

def draw_box(row, col, w, h, box_color, outer_color, label=""):
    """
    Draw a filled rounded box using full-size arcs.
      w, h        — total size including corner cells
      box_color   — 256-color index for the box interior
      outer_color — 256-color index for the surrounding background
    """
    buf = []

    corner_prefix = bold() + fg256(box_color) + bg256(outer_color)
    fill           = bg256(box_color) + fg256(outer_color)

    for r in range(h):
        buf.append(cup(row + r, col))
        if r == 0:
            # top row: TL corner, top edge, TR corner
            buf.append(corner_prefix + FULL_TL)
            buf.append(fill + " " * (w - 2))
            buf.append(corner_prefix + FULL_TR)
        elif r == h - 1:
            # bottom row: BL corner, bottom edge, BR corner
            buf.append(corner_prefix + FULL_BL)
            buf.append(fill + " " * (w - 2))
            buf.append(corner_prefix + FULL_BR)
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

    corner_prefix = bold() + fg256(box_color) + bg256(outer_color)
    fill = bg256(box_color) + fg256(outer_color)

    for r in range(actual_height):
        buf.append(cup(row + r, col))
        logical_row = r // 2
        is_upper_half = (r % 2 == 0)

        if logical_row == 0:
            # Top row: upper half uses HALF_TL, lower half is empty
            if is_upper_half:
                buf.append(corner_prefix + HALF_TL)
                buf.append(fg256(box_color) + bg256(outer_color) + LOWER_HALF * (w - 2))
                buf.append(corner_prefix + HALF_TR)
            else:
                buf.append(fill + LEFT_HALF)
                buf.append(fill + " " * (w - 2))
                buf.append(fill + RIGHT_HALF)

        elif logical_row == h_half - 1:
            # Bottom row: upper half is box color, lower half uses HALF_BL/HALF_BR
            if is_upper_half:
                # Upper half of bottom row: use LEFT_HALF and RIGHT_HALF for the sides
                buf.append(fill + LEFT_HALF +
                          fill + " " * (w - 2) +
                          fill + RIGHT_HALF)
            else:
                buf.append(corner_prefix + HALF_BL)
                buf.append(bg256(box_color) + fg256(outer_color) + LOWER_HALF * (w - 2))
                buf.append(corner_prefix + HALF_BR)

        else:
            buf.append(fill + LEFT_HALF +
                      fill + " " * (w - 2) +
                      fill + RIGHT_HALF)


    buf.append(reset())
    return "".join(buf)

out = []
out.append(clear())
out.append(cup(1, 1))

# Box 1 — steel blue on default background (full-size arcs)
out.append(draw_box(2, 4, 32, 7, box_color=24, outer_color=0,
                    label="full-size arcs"))

# Box 2 — dark red on default background (full-size arcs)
out.append(draw_box(11, 4, 32, 7, box_color=88, outer_color=0,
                    label="another box"))

# Box 3 — nested: green box on blue background (demonstrates corners blend)
out.append(bg256(24) + "".join(
    cup(20 + r, 4) + " " * 40 for r in range(9)
))
out.append(draw_box(21, 6, 28, 7, box_color=22, outer_color=24,
                    label="nested box"))

# Box 4 — magenta using half-size arcs (positioned below box 3)
out.append(draw_halfsize_box(33, 4, 32, 5, box_color=127, outer_color=0,
                             label="half-size arcs"))

# Box 5 — cyan using half-size arcs, nested on magenta background
out.append(bg256(127) + "".join(
    cup(44 + r, 4) + " " * 32 for r in range(10)
))
out.append(draw_halfsize_box(45, 6, 28, 4, box_color=51, outer_color=127,
                             label="nested half"))

out.append(reset())
out.append(cup(50, 1))

sys.stdout.write("".join(out))
sys.stdout.flush()
