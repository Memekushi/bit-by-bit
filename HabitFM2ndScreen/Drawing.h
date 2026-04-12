#ifndef DRAWING_H
#define DRAWING_H

#include "Config.h"

// Reference variables that are actually stored in the .ino file
extern int curX, curY;
extern int ringsOnScreen;
extern uint16_t ringPalette[12];
extern DrawingType activeDrawing;

inline int getDaysInMonth(int m, int y) {
  if (m == 2) return (((y % 4 == 0) && (y % 100 != 0)) || (y % 400 == 0)) ? 29 : 28;
  if (m == 4 || m == 6 || m == 9 || m == 11) return 30;
  return 31;
}

inline void drawUltraThickCheckmark(int x, int y, uint16_t color) {
  for (int i = -3; i <= 3; i++) {
    tft.drawLine(x - 18, y + i, x - 6, y + 12 + i, color); 
    tft.drawLine(x - 6, y + 12 + i, x + 18, y - 12 + i, color);  
  }
}

inline void drawPhysicalRingSegment(int index) {
  float catAngle = (index * 45.0 - 90.0);
  for (int r = 70; r < 82; r++) { 
    int pIdx = r - 70;
    uint16_t color = (pIdx < 4) ? ringPalette[2] : (pIdx > 8 ? ringPalette[10] : ringPalette[6]);
    for (float a = catAngle - 25; a <= catAngle + 25; a += 0.5) {
      float rad = a * PI / 180.0;
      int px = CIRCLE_X + cos(rad) * r;
      int py = CIRCLE_Y + sin(rad) * r;
      tft.drawPixel(px, py, color);
    }
  }
}

void GIFDraw(GIFDRAW *pDraw); // Declaration only, logic stays in .ino or a .cpp

#endif