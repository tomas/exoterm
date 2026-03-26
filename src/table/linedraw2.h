static uint32_t linedraw2_command[] = {
  0x01000084, // [0]  2b12 - square with top half black
  0x01000488, // [1]  2b13 - square with bottom half black
  0x01300088, // [2]  2b1a - dotted square (25% stipple)
  0x01000088, // [3]  2b1b - black large square (full fill)
  0x00000080, // [4]  2b1c - white large square: top edge
  0x00000888, // [5]         bottom edge
  0x00000008, // [6]         left edge
  0x00008088, // [7]         right edge
  0x01003455, // [8]  2b1d - black very small square (x:3-5, y:4-5)
  0x00003454, // [9]  2b1e - white very small square: top edge
  0x00003555, // [10]        bottom edge
  0x00003435, // [11]        left edge
  0x00005455, // [12]        right edge
  0x04150088, // [13] 2b24 - black large circle (filled, bbox 0,0→8,8)
};

static uint16_t linedraw2_offs[] = {
  0x0000, 0x0000, 0x0000, 0x0000, // 2b00-2b03
  0x0000, 0x0000, 0x0000, 0x0000, // 2b04-2b07
  0x0000, 0x0000, 0x0000, 0x0000, // 2b08-2b0b
  0x0000, 0x0000, 0x0000, 0x0000, // 2b0c-2b0f
  0x0000, 0x0000,                  // 2b10-2b11
  0x0001,                          // 2b12 square with top half black
  0x0011,                          // 2b13 square with bottom half black
  0x0000, 0x0000, 0x0000, 0x0000, // 2b14-2b17
  0x0000, 0x0000,                  // 2b18-2b19
  0x0021,                          // 2b1a dotted square
  0x0031,                          // 2b1b black large square
  0x0044,                          // 2b1c white large square (4 cmds)
  0x0081,                          // 2b1d black very small square
  0x0094,                          // 2b1e white very small square (4 cmds)
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, // 2b1f-2b23
  0x00d1,                          // 2b24 large circle
};
