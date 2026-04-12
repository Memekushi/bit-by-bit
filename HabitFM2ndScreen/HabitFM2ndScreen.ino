#include "Config.h"
#include "Drawing.h"
// Your GIF header files go here
#include "victory_gif.h" 
#include "fire_gif.h"
#include "nuko_leader.h"
#include "nuko_squad.h"

// --- DEFINE GLOBAL OBJECTS (Actually allocate memory) ---
TFT_eSPI tft = TFT_eSPI();
RTC_DS3231 rtc;
AnimatedGIF gifVictory, gifFire, gifLeader, gifSquad;

// --- STATE VARIABLES ---
AppMode currentMode = DASHBOARD;
DrawingType activeDrawing;
int curX, curY; 
int xPos[9], yPos[9];
unsigned long fireTimer = 0, leaderTimer = 0, squadTimer = 0;
int fireDelay = 0, leaderDelay = 0, squadDelay = 0;
uint8_t lastSec = 99; 
int catsOnScreen = 0; 
int ringsOnScreen = 0;
unsigned long joinTimer = 0;
const int JOIN_DELAY = 2000;
int victoryLoopCount = 0;
unsigned long victoryTimer = 0;
uint16_t ringPalette[12];
const char* daysOfWeek[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

// Implement the GIFDraw logic here
void GIFDraw(GIFDRAW *pDraw) {
  uint16_t *usPalette = pDraw->pPalette;
  uint8_t ucTransparent = pDraw->ucTransparent;
  static uint16_t lineBuf[320];

  int y = (activeDrawing == DRAW_VICTORY) ? (pDraw->iY + pDraw->y) : (curY + pDraw->iY + pDraw->y);
  if (y < 0 || y >= 240) return;

  for (int i = 0; i < pDraw->iWidth; i++) {
    int gx = (activeDrawing == DRAW_VICTORY) ? (pDraw->iX + i) : (curX + pDraw->iX + i);
    if (activeDrawing != DRAW_FIRE && gx >= BORDER_X) continue;
    uint8_t idx = pDraw->pPixels[i];
    uint16_t pCol = usPalette[idx];
    if (idx == ucTransparent || pCol == 0xF81F) {
      if (activeDrawing == DRAW_VICTORY || activeDrawing == DRAW_FIRE) { lineBuf[i] = BG_BROWN; }
      else {
        long dx = gx - CIRCLE_X; long dy = y - CIRCLE_Y;
        long dSq = (dx * dx) + (dy * dy);
        if (dSq >= 4900 && dSq <= 6724) {
          float angle = atan2((float)dy, (float)dx) * (180.0 / PI);
          bool pixelInActiveRing = false;
          for (int k = 0; k < ringsOnScreen; k++) {
            float catAngle = (k * 45.0 - 90.0);
            float diff = angle - catAngle;
            while (diff > 180) diff -= 360;
            while (diff < -180) diff += 360;
            if (abs(diff) <= 26.0) { pixelInActiveRing = true; break; }
          }
          if (pixelInActiveRing) {
            if (dSq < 5329) lineBuf[i] = ringPalette[2];
            else if (dSq > 6241) lineBuf[i] = ringPalette[10]; 
            else lineBuf[i] = ringPalette[6];
          } else lineBuf[i] = BG_BROWN;
        } else lineBuf[i] = BG_BROWN;
      }
    } else lineBuf[i] = pCol;
  }
  int drawX = (activeDrawing == DRAW_VICTORY) ? pDraw->iX : (curX + pDraw->iX);
  int drawW = pDraw->iWidth;
  if (activeDrawing != DRAW_FIRE && drawX + drawW > BORDER_X) drawW = BORDER_X - drawX;
  if (drawW > 0) tft.pushImage(drawX, y, drawW, 1, lineBuf);
}

void setup() {
  tft.begin(); tft.setRotation(1); tft.setSwapBytes(true);
  tft.fillScreen(BG_BROWN); 
  if (rtc.begin()) { if (rtc.lostPower()) { rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); } }
  
  for (int i = 0; i < 12; i++) {
    float r1 = (RING_LEAF >> 11) & 0x1F, g1 = (RING_LEAF >> 5) & 0x3F, b1 = RING_LEAF & 0x1F;
    float r2 = (RING_SAGE >> 11) & 0x1F, g2 = (RING_SAGE >> 5) & 0x3F, b2 = RING_SAGE & 0x1F;
    float f = (i < 6) ? (i / 6.0) : (1.0 - (i - 6) / 6.0);
    ringPalette[i] = ((uint16_t)(r1+(r2-r1)*f)<<11) | ((uint16_t)(g1+(g2-g1)*f)<<5) | (uint16_t)(b1+(b2-b1)*f);
  }
  for (int i = 0; i < 8; i++) {
    float a = (i * 45 - 90) * (PI / 180.0);
    xPos[i] = (int)(CIRCLE_X + cos(a) * 75) - (i == 0 ? 18 : 29);
    yPos[i] = (int)(CIRCLE_Y + sin(a) * 75) - 24;
  }
  xPos[8] = PANEL_CENTER - 32; yPos[8] = 85; 

  gifFire.begin(LITTLE_ENDIAN_PIXELS); gifLeader.begin(LITTLE_ENDIAN_PIXELS); 
  gifSquad.begin(LITTLE_ENDIAN_PIXELS); gifVictory.begin(LITTLE_ENDIAN_PIXELS);
  gifFire.open((uint8_t *)fire_gif, sizeof(fire_gif), GIFDraw);
  gifLeader.open((uint8_t *)nuko_leader, sizeof(nuko_leader), GIFDraw);
  gifSquad.open((uint8_t *)nuko_squad, sizeof(nuko_squad), GIFDraw);
  joinTimer = millis() + 500;
}

void loop() {
  unsigned long now = millis();
  DateTime rtcNow = rtc.now();

  // GLOBAL FIRE
  if (now >= fireTimer) { 
    activeDrawing = DRAW_FIRE; curX = xPos[8]; curY = yPos[8]; 
    if (gifFire.playFrame(false, &fireDelay)) fireTimer = now + fireDelay; 
  }

  if (currentMode == DASHBOARD || currentMode == VICTORY_WAIT) {
    if (rtcNow.second() != lastSec) {
      tft.fillRect(BORDER_X, 0, BRD_THICK, 240, RING_LEAF); 
      tft.fillRect(BORDER_X + BRD_THICK, 75, 320 - BORDER_X - BRD_THICK, BRD_THICK, RING_LEAF);
      tft.setTextColor(TEXT_PARCH, BG_BROWN); 
      tft.drawCentreString(String(rtcNow.month()) + "/" + String(rtcNow.day()), PANEL_CENTER, 10, 4);
      tft.drawCentreString(daysOfWeek[rtcNow.dayOfTheWeek() % 7], PANEL_CENTER, 42, 4); 
      drawUltraThickCheckmark(PANEL_CENTER, 165, RING_LEAF); 
      tft.setTextColor(TEXT_LEAF, BG_BROWN); 
      tft.drawCentreString("Days", PANEL_CENTER, 185, 2); 
      tft.setTextColor(TEXT_PARCH, BG_BROWN); 
      int daysInMonth = getDaysInMonth(rtcNow.month(), rtcNow.year());
      tft.drawCentreString("1/" + String(daysInMonth), PANEL_CENTER, 205, 4); 
      lastSec = rtcNow.second();
    }

    if (currentMode == DASHBOARD && now >= joinTimer && catsOnScreen < 8) {
      drawPhysicalRingSegment(catsOnScreen);
      ringsOnScreen++; catsOnScreen++;  
      joinTimer = now + JOIN_DELAY;
      if (catsOnScreen == 8) { currentMode = VICTORY_WAIT; victoryTimer = millis() + 1000; }
      lastSec = 99; 
    }

    if (catsOnScreen >= 1 && now >= leaderTimer) { 
      activeDrawing = DRAW_LEADER; curX = xPos[0]; curY = yPos[0]; 
      if (gifLeader.playFrame(false, &leaderDelay)) leaderTimer = now + leaderDelay; 
    }
    if (catsOnScreen >= 2 && now >= squadTimer) { 
      activeDrawing = DRAW_SQUAD; 
      for (int i = 1; i < catsOnScreen; i++) { curX = xPos[i]; curY = yPos[i]; gifSquad.playFrame(false, &squadDelay); }
      squadTimer = now + squadDelay; 
    }
    
    if (currentMode == VICTORY_WAIT && millis() >= victoryTimer) {
      tft.fillRect(0, 0, BORDER_X, 240, BG_BROWN); 
      victoryLoopCount = 0; activeDrawing = DRAW_VICTORY;
      gifVictory.open((uint8_t *)victory_gif, victory_gif_size, GIFDraw); 
      gifVictory.playFrame(false, NULL); delay(1000); 
      currentMode = VICTORY_PLAY;
    }
  }
  else if (currentMode == VICTORY_PLAY) {
    activeDrawing = DRAW_VICTORY;
    if (!gifVictory.playFrame(false, NULL)) {
      victoryLoopCount++; gifVictory.close();
      if (victoryLoopCount < 3) {
        gifVictory.open((uint8_t *)victory_gif, victory_gif_size, GIFDraw);
        gifVictory.playFrame(false, NULL); delay(1000); 
      } else {
        gifVictory.open((uint8_t *)victory_gif, victory_gif_size, GIFDraw);
        gifVictory.playFrame(false, NULL); gifVictory.close(); delay(2000); 
        currentMode = DASHBOARD; tft.fillScreen(BG_BROWN);
        for (int i = 0; i < 8; i++) drawPhysicalRingSegment(i);
        gifFire.close(); gifLeader.close(); gifSquad.close();
        gifFire.open((uint8_t *)fire_gif, sizeof(fire_gif), GIFDraw);
        gifLeader.open((uint8_t *)nuko_leader, sizeof(nuko_leader), GIFDraw);
        gifSquad.open((uint8_t *)nuko_squad, sizeof(nuko_squad), GIFDraw);
        lastSec = 99; 
      }
    }
  }
}