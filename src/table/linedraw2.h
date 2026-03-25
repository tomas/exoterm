static uint32_t linedraw2_command[] = {
  0x01000088, // 2b1d - filled rectangle (black very small square)
  0x01000088, // 2b1e - filled rectangle (white very small square)
  0x01000088, // 2b1f - filled rectangle
  0x01000088, // 2b20 - filled rectangle
  0x01000088, // 2b21 - filled rectangle
  0x01000088, // 2b22 - filled rectangle
  0x01000088, // 2b23 - filled rectangle
  0x01000088, // 2b24 - filled rectangle (black lozenge)
};

static uint16_t linedraw2_offs[] = {
  0x0000, 0x0000, 0x0000, 0x0000, // 2b00-2b03
  0x0000, 0x0000, 0x0000, 0x0000, // 2b04-2b07
  0x0000, 0x0000, 0x0000, 0x0000, // 2b08-2b0b
  0x0000, 0x0000, 0x0000, 0x0000, // 2b0c-2b0f
  0x0000, 0x0000, 0x0000, 0x0000, // 2b10-2b13
  0x0000, 0x0000, 0x0000, 0x0000, // 2b14-2b17
  0x0000, 0x0000, 0x0000, 0x0000, // 2b18-2b1b
  0x0000, 0x0000,                   // 2b1c-2b1d
  0x0001, // 2b1d - start=0, count=1
  0x0011, // 2b1e - start=1, count=1
  0x0021, // 2b1f - start=2, count=1
  0x0031, // 2b20 - start=3, count=1
  0x0041, // 2b21 - start=4, count=1
  0x0051, // 2b22 - start=5, count=1
  0x0061, // 2b23 - start=6, count=1
  0x0071, // 2b24 - start=7, count=1
};
