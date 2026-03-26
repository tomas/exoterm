// op=0: line x1,y1→x2,y2
// op=1: filled rect x1,y1→x2,y2 (a=0 solid, a=1 75%, a=2 50%, a=3 25% stipple)
// op=3: filled triangle, a=1 up / 2 down / 3 left / 4 right, bbox x1,y1→x2,y2
// op=4: filled arc, bbox x1,y1→x2,y2, a=start/90° (1-based), b=span/90° (1-based)
// coords: 0=left/top, 8=right/bottom, 10=center, 9=center-1, 11=center+1
// glyphs are inset 1px top and bottom (y=1..7 instead of y=0..8)
// offset = (start_index << 4) | count
static uint32_t linedraw4_command[] = {
  0x01000187, // [0]  25a0 black square: filled rect 0,1→8,7

  0x00000181, // [1]  25a1 white square: top edge 0,1→8,1
  0x00000787, // [2]        bottom edge 0,7→8,7
  0x00000107, // [3]        left edge   0,1→0,7
  0x00008187, // [4]        right edge  8,1→8,7

  0x00001171, // [5]  25a2 white square with rounded corners: top 1,1→7,1
  0x00001777, // [6]        bottom 1,7→7,7
  0x00000107, // [7]        left   0,1→0,7
  0x00008187, // [8]        right  8,1→8,7

  0x00000181, // [9]  25a3 white square containing black small square: outline top
  0x00000787, // [10]       outline bottom
  0x00000107, // [11]       outline left
  0x00008187, // [12]       outline right
  0x01003355, // [13]       inner filled rect 3,3→5,5

  0x00000181, // [14] 25a4 square with horizontal fill: outline top
  0x00000787, // [15]       outline bottom
  0x00000107, // [16]       outline left
  0x00008187, // [17]       outline right
  0x00000282, // [18]       horizontal line at y=2
  0x00000484, // [19]       horizontal line at y=4
  0x00000686, // [20]       horizontal line at y=6

  0x00000181, // [21] 25a5 square with vertical fill: outline top
  0x00000787, // [22]       outline bottom
  0x00000107, // [23]       outline left
  0x00008187, // [24]       outline right
  0x00002127, // [25]       vertical line at x=2, y=1→7
  0x00004147, // [26]       vertical line at x=4, y=1→7
  0x00006167, // [27]       vertical line at x=6, y=1→7

  0x00000181, // [28] 25a6 square with orthogonal crosshatch fill: outline top
  0x00000787, // [29]       outline bottom
  0x00000107, // [30]       outline left
  0x00008187, // [31]       outline right
  0x00000282, // [32]       horizontal line at y=2
  0x00000484, // [33]       horizontal line at y=4
  0x00000686, // [34]       horizontal line at y=6
  0x00002127, // [35]       vertical line at x=2, y=1→7
  0x00004147, // [36]       vertical line at x=4, y=1→7
  0x00006167, // [37]       vertical line at x=6, y=1→7

  0x00000181, // [38] 25a7 square with upper-left to lower-right fill (\): outline top
  0x00000787, // [39]       outline bottom
  0x00000107, // [40]       outline left
  0x00008187, // [41]       outline right
  0x00000187, // [42]       \ diagonal 0,1→8,7
  0x00002186, // [43]       \ diagonal 2,1→8,6
  0x00000267, // [44]       \ diagonal 0,2→6,7
  0x00004184, // [45]       \ diagonal 4,1→8,4
  0x00000447, // [46]       \ diagonal 0,4→4,7

  0x00000181, // [47] 25a8 square with upper-right to lower-left fill (/): outline top
  0x00000787, // [48]       outline bottom
  0x00000107, // [49]       outline left
  0x00008187, // [50]       outline right
  0x00000781, // [51]       / diagonal 0,7→8,1
  0x00002782, // [52]       / diagonal 2,7→8,2
  0x00000661, // [53]       / diagonal 0,6→6,1
  0x00004784, // [54]       / diagonal 4,7→8,4
  0x00000441, // [55]       / diagonal 0,4→4,1

  0x00000181, // [56] 25a9 square with diagonal crosshatch fill: outline top
  0x00000787, // [57]       outline bottom
  0x00000107, // [58]       outline left
  0x00008187, // [59]       outline right
  0x00000187, // [60]       \ diagonal 0,1→8,7
  0x00002186, // [61]       \ diagonal 2,1→8,6
  0x00000267, // [62]       \ diagonal 0,2→6,7
  0x00004184, // [63]       \ diagonal 4,1→8,4
  0x00000447, // [64]       \ diagonal 0,4→4,7
  0x00000781, // [65]       / diagonal 0,7→8,1
  0x00002782, // [66]       / diagonal 2,7→8,2
  0x00000661, // [67]       / diagonal 0,6→6,1
  0x00004784, // [68]       / diagonal 4,7→8,4
  0x00000441, // [69]       / diagonal 0,4→4,1

  0x01002266, // [70] 25aa black small square: filled rect 2,2→6,6

  0x00002262, // [71] 25ab white small square: top edge 2,2→6,2
  0x00002666, // [72]       bottom edge 2,6→6,6
  0x00002226, // [73]       left edge   2,2→2,6
  0x00006266, // [74]       right edge  6,2→6,6

  0x01000286, // [75] 25ac black rectangle: filled rect 0,2→8,6

  0x00000282, // [76] 25ad white rectangle: top edge 0,2→8,2
  0x00000686, // [77]       bottom edge 0,6→8,6
  0x00000206, // [78]       left edge   0,2→0,6
  0x00008286, // [79]       right edge  8,2→8,6

  0x01002167, // [80] 25ae black vertical rectangle: filled rect 2,1→6,7

  0x00002161, // [81] 25af white vertical rectangle: top edge 2,1→6,1
  0x00002767, // [82]       bottom edge 2,7→6,7
  0x00002127, // [83]       left edge   2,1→2,7
  0x00006167, // [84]       right edge  6,1→6,7
};

static uint16_t linedraw4_offs[] = {
  0x0001, // 25a0 black square
  0x0014, // 25a1 white square
  0x0054, // 25a2 white square with rounded corners
  0x0095, // 25a3 white square containing black small square
  0x00e7, // 25a4 square with horizontal fill
  0x0157, // 25a5 square with vertical fill
  0x01ca, // 25a6 square with orthogonal crosshatch fill
  0x0269, // 25a7 square with upper left to lower right fill
  0x02f9, // 25a8 square with upper right to lower left fill
  0x038e, // 25a9 square with diagonal crosshatch fill
  0x0461, // 25aa black small square
  0x0474, // 25ab white small square
  0x04b1, // 25ac black rectangle
  0x04c4, // 25ad white rectangle
  0x0501, // 25ae black vertical rectangle
  0x0514, // 25af white vertical rectangle
};
