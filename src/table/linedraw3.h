// op=3: filled triangle, a=1 up / 2 down / 3 left / 4 right, bbox x1,y1→x2,y2
// op=4: filled arc, bbox x1,y1→x2,y2, a=start/90° (1-based), b=span/90° (1-based)
static uint32_t linedraw3_command[] = {
  0x03301177, // [0] 23f4 black medium left-pointing triangle
  0x03401177, // [1] 23f5 black medium right-pointing triangle
  0x03101177, // [2] 23f6 black medium up-pointing triangle
  0x03201177, // [3] 23f7 black medium down-pointing triangle
  0x01002137, // [4] 23f8 double vertical bar (pause) - left bar
  0x01005167, // [5]       right bar
  0x01001177, // [6] 23f9 black medium square (stop)
  0x04151177, // [7] 23fa black medium circle (record)
};

// offset = (start_index << 4) | count
static uint16_t linedraw3_offs[] = {
  0x0001, // 23f4 left triangle
  0x0011, // 23f5 right triangle
  0x0021, // 23f6 up triangle
  0x0031, // 23f7 down triangle
  0x0042, // 23f8 double vertical bar (2 cmds)
  0x0061, // 23f9 medium square
  0x0071, // 23fa medium circle
};
