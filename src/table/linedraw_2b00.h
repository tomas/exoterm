static uint32_t linedraw2_command[] = {
   0x00000181, // [0]  2b12 outline: top edge 0,1→8,1
   0x00000787, // [1]         bottom edge 0,7→8,7
   0x00000107, // [2]         left edge   0,1→0,7
   0x00008187, // [3]         right edge  8,1→8,7
   0x01000184, // [4]  2b12 top half: filled rect 0,1→8,4

   0x01000487, // [5]  2b13 bottom half: filled rect 0,4→8,7
   0x00000181, // [6]  2b13 outline: top edge 0,1→8,1
   0x00000787, // [7]         bottom edge 0,7→8,7
   0x00000107, // [8]         left edge   0,1→0,7
   0x00008187, // [9]         right edge  8,1→8,7

   0x01300088, // [10] 2b1a - dotted square (25% stipple)
   0x01000088, // [11] 2b1b - black large square (full fill)
   0x00000080, // [12] 2b1c - white large square: top edge
   0x00000888, // [13]        bottom edge
   0x00000008, // [14]        left edge
   0x00008088, // [15]        right edge
   0x01003455, // [16] 2b1d - black very small square (x:3-5, y:4-5)
   0x00003454, // [17] 2b1e - white very small square: top edge
   0x00003555, // [18]        bottom edge
   0x00003435, // [19]        left edge
   0x00005455, // [20]        right edge
   0x0640AA00, // [21] 2b24 - black large circle (filled at center 10,10, a=4)
};

static uint16_t linedraw2_offs[] = {
  0x0000, 0x0000, 0x0000, 0x0000, // 2b00-2b03
  0x0000, 0x0000, 0x0000, 0x0000, // 2b04-2b07
  0x0000, 0x0000, 0x0000, 0x0000, // 2b08-2b0b
  0x0000, 0x0000, 0x0000, 0x0000, // 2b0c-2b0f
  0x0000, 0x0000,                  // 2b10-2b11
   0x0005,                          // 2b12 square with top half black (5 cmds)
   0x0055,                          // 2b13 square with bottom half black (5 cmds)
   0x0000, 0x0000, 0x0000, 0x0000, // 2b14-2b17
   0x0000, 0x0000,                  // 2b18-2b19
   0x00a1,                          // 2b1a dotted square
   0x00b1,                          // 2b1b black large square
   0x00c4,                          // 2b1c white large square (4 cmds)
   0x0101,                          // 2b1d black very small square
   0x0114,                          // 2b1e white very small square (4 cmds)
   0x0000, 0x0000, 0x0000, 0x0000, 0x0000, // 2b1f-2b23
   0x0151,                          // 2b24 large circle
};
