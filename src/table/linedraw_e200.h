// U+E200..U+E203 — filled rounded-corner sector glyphs (Private Use Area)
//
// Each glyph fills the interior quadrant of a rounded-box corner cell using
// op=5 (filled sector). The sector is a quarter-circle whose arc matches the
// outline drawn by U+256D–U+2570, but filled rather than stroked. Because the
// filled sector of a circle centred at the cell corner lies entirely within the
// cell, no clip mask is needed.
//
// Usage:  fg = box background colour,  bg = outer background colour.
//   U+E200  ╭-fill  top-left  corner cell  (sector 270°..360°, lower-right)
//   U+E201  ╮-fill  top-right corner cell  (sector 180°..270°, lower-left)
//   U+E202  ╰-fill  bot-left  corner cell  (sector   0°.. 90°, upper-right)
//   U+E203  ╯-fill  bot-right corner cell  (sector  90°..180°, upper-left)
//
// Command word:  op[31:24] a[23:20] b[19:16] x1[15:12] y1[11:8] x2[7:4] y2[3:0]
//   op = 5, a = corner (1-4), remaining fields unused.

static uint32_t linedraw_e200_command[] = {
  0x05100000, // E200  top-left  corner fill
  0x05200000, // E201  top-right corner fill
  0x05300000, // E202  bot-left  corner fill
  0x05400000, // E203  bot-right corner fill
};

// offs encoding: high 12 bits = start index, low 4 bits = command count
static uint16_t linedraw_e200_offs[] = {
  0x0001, // E200: index 0, 1 command
  0x0011, // E201: index 1, 1 command
  0x0021, // E202: index 2, 1 command
  0x0031, // E203: index 3, 1 command
};
