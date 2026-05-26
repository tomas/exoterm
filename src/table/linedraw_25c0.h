// op=2: arc outline, center at x1,y1, full cell size
// op=4: filled arc, bbox x1,y1→x2,y2, a=start/90° (1-based), b=span/90° (1-based)
// coords: 0=left/top, 8=right/bottom, 10=center, 9=center-1, 11=center+1
// offset = (start_index << 4) | count
static uint32_t linedraw6_command[] = {
  0x0215AA00, // [0] 25c9/25cb white circle: op=2 full arc centered at 10,10
  0x04154466, // [1] 25c9 center dot: filled arc bbox 4,4→6,6
  0x0640AA00, // [2] 25cf black circle: filled at center 10,10, a=4
  0x0620AA00, // [3] 25d8 inverse bullet: filled at center 10,10, a=2 (half)
  0x0215AA00, // [4] 25d0 circle outline (dup): op=2 full arc centered at 10,10
  0x04430088, // [5] 25d0 left half: op=4 fill 270°→450° bbox 0,0→8,8
  0x0215AA00, // [6] 25d1 circle outline (dup): op=2 full arc centered at 10,10
  0x04230088, // [7] 25d1 right half: op=4 fill 90°→270° bbox 0,0→8,8
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
   0x0042,                         // 25d0 circle with left half black (2 cmds)
   0x0062,                         // 25d1 circle with right half black (2 cmds)
   0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, // 25d2-25d7
  0x0031,                         // 25d8 inverse bullet (1 cmd)
};
