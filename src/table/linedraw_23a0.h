// op=0: line x1,y1→x2,y2
// coords: 0=left/top, 8=right/bottom, 10=center, 9=center-1, 11=center+1
// glyphs y=1..7 bracket elements are inset 1px top and bottom
// offset = (start_index << 4) | count
static uint32_t linedraw5_command[] = {
  0x00008188, // [0] 23a4 vertical: x=8, y=1→8
  0x00004181, // [1] 23a4 horizontal: y=1, x=4→8
  0x00008088, // [2] 23a5 vertical extension: x=8, y=0→8
  0x00008087, // [3] 23a6 vertical: x=8, y=0→7
  0x00004787, // [4] 23a6 horizontal: y=7, x=4→8
  0x00000484, // [5] 23af horizontal line: y=4, x=0→8
};

// offset = (start_index << 4) | count
static uint16_t linedraw5_offs[] = {
  0x0000, 0x0000, 0x0000, 0x0000, // 23a0-23a3
  0x0002,                         // 23a4 right bracket upper corner (2 cmds)
  0x0021,                         // 23a5 right bracket extension (1 cmd)
  0x0032,                         // 23a6 right bracket lower corner (2 cmds)
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, // 23a7-23ae
  0x0051,                         // 23af horizontal line extension (1 cmd)
};
