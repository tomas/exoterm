// op=2: arc outline, center at x1,y1, full cell size
// op=4: filled arc, bbox x1,y1→x2,y2, a=start/90° (1-based), b=span/90° (1-based)
// coords: 0=left/top, 8=right/bottom, 10=center, 9=center-1, 11=center+1
// offset = (start_index << 4) | count
static uint32_t linedraw6_command[] = {
  0x0215AA00, // [0] 25c9/25cb white circle: op=2 full arc centered at 10,10
  0x04154466, // [1] 25c9 center dot: filled arc bbox 4,4→6,6
  0x04151177, // [2] 25cf black circle: filled arc bbox 1,1→7,7
  0x04152255, // [3] 25d8 inverse bullet: filled arc bbox 2,2→5,5
};

// offset = (start_index << 4) | count
static uint16_t linedraw6_offs[] = {
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, // 25c0-25c8
  0x0002,                         // 25c9 fisheye (2 cmds: circle + dot)
  0x0000,                         // 25ca
  0x0001,                         // 25cb white circle (1 cmd)
  0x0000, 0x0000,                 // 25cc-25cd
  0x0000,                         // 25ce bullseye (unsupported)
  0x0021,                         // 25cf black circle (1 cmd)
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, // 25d0-25d7
  0x0031,                         // 25d8 inverse bullet (1 cmd)
};
