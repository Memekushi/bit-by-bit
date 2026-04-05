#include <TFT_eSPI.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

TFT_eSPI tft = TFT_eSPI();

#define TFT_BL 21

// Touchscreen pins
#define TOUCH_CS   33
#define TOUCH_IRQ  36   // VP
#define TOUCH_SCK  25
#define TOUCH_MOSI 32
#define TOUCH_MISO 39   // VN

SPIClass touchSPI(VSPI);
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

// Screen
const int SCREEN_W = 320;
const int SCREEN_H = 240;

// Layout
const int CARD_X = 10;
const int CARD_W = 300;
const int CARD_H = 38;

const int ICON_X_OFFSET = 16;
const int LABEL_X_OFFSET = 42;

const int CHECKBOX_SIZE = 18;
const int CHECKBOX_X = 276;

// Progress bar
const int TITLE_Y = 8;
const int BAR_X = 95;
const int BAR_Y = 14;
const int BAR_W = 145;
const int BAR_H = 10;
const int BAR_GAP = 8;

// Touch calibration from your real readings
// rawX controls vertical placement
// rawY controls horizontal placement
const int RAW_X_TOP    = 2387;
const int RAW_X_BOTTOM = 633;

const int RAW_Y_LEFT   = 500;
const int RAW_Y_RIGHT  = 3500;

struct HabitBox {
  int x;
  int y;
  int w;
  int h;
  const char* label;
};

HabitBox habits[4] = {
  {CARD_X, 40,  CARD_W, CARD_H, "Drink Water"},
  {CARD_X, 88,  CARD_W, CARD_H, "Read 10 Min"},
  {CARD_X, 136, CARD_W, CARD_H, "Work Out"},
  {CARD_X, 184, CARD_W, CARD_H, "Do Homework"}
};

bool checked[4] = {false, false, false, false};
bool touchLatch = false;

// ---------- Icons ----------
void drawWaterDropIcon(int x, int y) {
  tft.fillCircle(x, y + 3, 6, TFT_BLUE);
  tft.fillTriangle(x, y - 9, x - 6, y + 1, x + 6, y + 1, TFT_BLUE);
}

void drawBookIcon(int x, int y) {
  tft.fillRoundRect(x - 8, y - 9, 16, 18, 2, TFT_BLUE);
  tft.drawFastVLine(x, y - 9, 18, TFT_WHITE);
  tft.drawFastHLine(x - 5, y - 3, 3, TFT_WHITE);
  tft.drawFastHLine(x + 2, y - 3, 3, TFT_WHITE);
  tft.drawFastHLine(x - 5, y + 2, 3, TFT_WHITE);
  tft.drawFastHLine(x + 2, y + 2, 3, TFT_WHITE);
}

void drawDumbbellIcon(int x, int y) {
  tft.fillRect(x - 8, y - 2, 16, 4, TFT_BLUE);
  tft.fillRect(x - 12, y - 6, 3, 12, TFT_BLUE);
  tft.fillRect(x - 8,  y - 5, 2, 10, TFT_BLUE);
  tft.fillRect(x + 9,  y - 6, 3, 12, TFT_BLUE);
  tft.fillRect(x + 6,  y - 5, 2, 10, TFT_BLUE);
}

void drawPencilIcon(int x, int y) {
  tft.fillRect(x - 7, y - 2, 12, 4, TFT_BLUE);
  tft.fillTriangle(x + 5, y - 4, x + 10, y, x + 5, y + 4, TFT_BLUE);
  tft.fillTriangle(x - 9, y - 3, x - 7, y, x - 9, y + 3, TFT_BLACK);
}

// ---------- UI pieces ----------
void drawCheckbox(int x, int y, int size, bool isChecked) {
  tft.fillRect(x, y, size, size, TFT_WHITE);
  tft.drawRect(x, y, size, size, TFT_BLACK);

  if (isChecked) {
    tft.drawLine(x + 3, y + 9,  x + 7,  y + 13, TFT_BLUE);
    tft.drawLine(x + 7, y + 13, x + 14, y + 4,  TFT_BLUE);
    tft.drawLine(x + 3, y + 10, x + 7,  y + 14, TFT_BLUE);
    tft.drawLine(x + 7, y + 14, x + 14, y + 5,  TFT_BLUE);
  }
}

void drawProgressBar(int completed, int total) {
  tft.drawRoundRect(BAR_X, BAR_Y, BAR_W, BAR_H, 5, TFT_BLACK);

  int innerX = BAR_X + 1;
  int innerY = BAR_Y + 1;
  int innerW = BAR_W - 2;
  int innerH = BAR_H - 2;

  tft.fillRoundRect(innerX, innerY, innerW, innerH, 4, TFT_WHITE);

  int fillW = (completed * innerW) / total;
  if (fillW > 0) {
    tft.fillRoundRect(innerX, innerY, fillW, innerH, 4, TFT_BLUE);
  }

  String fraction = String(completed) + "/" + String(total);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(fraction, BAR_X + BAR_W + BAR_GAP, BAR_Y + BAR_H / 2, 2);
}

void drawHabitBox(HabitBox h, int index, bool isChecked) {
  tft.fillRoundRect(h.x, h.y, h.w, h.h, 10, TFT_WHITE);
  tft.drawRoundRect(h.x, h.y, h.w, h.h, 10, TFT_BLACK);

  int centerY = h.y + h.h / 2;
  int iconX = h.x + ICON_X_OFFSET;

  if (index == 0) drawWaterDropIcon(iconX, centerY);
  if (index == 1) drawBookIcon(iconX, centerY);
  if (index == 2) drawDumbbellIcon(iconX, centerY);
  if (index == 3) drawPencilIcon(iconX, centerY);

  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(h.label, h.x + LABEL_X_OFFSET, centerY, 2);

  int checkboxY = h.y + (h.h - CHECKBOX_SIZE) / 2;
  drawCheckbox(CHECKBOX_X, checkboxY, CHECKBOX_SIZE, isChecked);
}

void drawUI() {
  tft.fillScreen(TFT_WHITE);

  int completed = 0;
  for (int i = 0; i < 4; i++) {
    if (checked[i]) completed++;
  }

  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Habits", 10, TITLE_Y, 4);

  drawProgressBar(completed, 4);

  for (int i = 0; i < 4; i++) {
    drawHabitBox(habits[i], i, checked[i]);
  }
}

// ---------- Touch handling ----------
bool getTouchPoint(int &screenX, int &screenY) {
  if (!ts.touched()) return false;

  TS_Point p = ts.getPoint();
  int rawX = p.x;
  int rawY = p.y;

  // X across the screen
  screenX = map(rawY, RAW_Y_LEFT, RAW_Y_RIGHT, 0, SCREEN_W);

  // Y down the habit list
  // Use actual row center range from your measurements:
  // top row center ~59, bottom row center ~203
  screenY = map(rawX, RAW_X_TOP, RAW_X_BOTTOM, 59, 203);

  screenX = constrain(screenX, 0, SCREEN_W - 1);
  screenY = constrain(screenY, 0, SCREEN_H - 1);

  return true;
}

int habitRowHitIndex(int tx, int ty) {
  for (int i = 0; i < 4; i++) {
    int top = habits[i].y - 6;
    int bottom = habits[i].y + habits[i].h + 6;

    if (tx >= habits[i].x && tx <= habits[i].x + habits[i].w &&
        ty >= top && ty <= bottom) {
      return i;
    }
  }
  return -1;
}

void setup() {
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  tft.init();
  tft.setRotation(1);

  touchSPI.begin(TOUCH_SCK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  ts.begin(touchSPI);
  ts.setRotation(0);

  drawUI();
}

void loop() {
  int tx, ty;
  bool touchedNow = getTouchPoint(tx, ty);

  if (touchedNow && !touchLatch) {
    int hit = habitRowHitIndex(tx, ty);

    if (hit != -1) {
      checked[hit] = !checked[hit];
      drawUI();
    }

    touchLatch = true;
  }

  if (!touchedNow) {
    touchLatch = false;
  }

  delay(30);
}
