#include <TFT_eSPI.h>
#include <SPI.h>
#include <Wire.h>
#include <RTClib.h>

TFT_eSPI tft = TFT_eSPI();
RTC_DS3231 rtc;

// ---------------- BUTTON PINS ----------------
#define MENU_BTN    19
#define NEXT_BTN    18
#define SELECT_BTN  5

// ---------------- RTC PINS ----------------
#define RTC_SDA     26
#define RTC_SCL     23

// ---------------- APP STATE ----------------
enum Page {
  PAGE_HABITS,
  PAGE_STATS,
  PAGE_SETTINGS
};

Page currentPage = PAGE_HABITS;

bool menuOpen = false;
int menuIndex = 0;

// ---------------- HABITS ----------------
const int habitCount = 7;

const char* habits[habitCount] = {
  "Drink Water",
  "Read 10 Min",
  "Work Out",
  "Homework",
  "Meditate",
  "Sleep Early",
  "Stretch"
};

int habitIndex = 0;
int scrollOffset = 0;
const int visibleHabits = 4;

// weeklyDone[habit][day]
// Monday=0 ... Sunday=6
bool weeklyDone[habitCount][7] = {false};

// ---------------- MENU ----------------
const char* menuItems[3] = {"Habits", "Stats", "Settings"};

// ---------------- BUTTON STATE ----------------
bool lastMenu = HIGH;
bool lastNext = HIGH;
bool lastSelect = HIGH;

unsigned long lastPress = 0;
const unsigned long debounceMs = 160;

// ---------------- COLORS ----------------
// Retro radio inspired: warm background, muted borders, deep navy accents
#define UI_WHITE        0xFF7A   // warm off-white / cream
#define UI_BG           0xFF7A
#define UI_CARD         0xFF9C   // slightly different warm panel
#define UI_BLACK        0x2124   // softer than pure black
#define UI_BLUE         0x19AA   // muted radio-blue
#define UI_DARKBLUE     0x10A6   // deeper navy-blue
#define UI_SOFTBLUE     0xDEFB   // pale blue selection fill
#define UI_BORDER       0xC618   // soft gray border
#define UI_BROWN        0x6A00   // dark brown
#define UI_TAPE_FILL    0xFF38   // highlight brown 
#define UI_TODAY_COL    0xFF59   // light gray highlight 
#define UI_LIGHTGRAY    0xDEDB
#define UI_MIDGRAY      0x94B2
#define UI_DARKGRAY     0x632C
#define UI_SIDEBAR      0xF6F7   // pale warm gray
#define UI_DATE_BG      0xEF3C   // LCD label style fill
#define UI_DATE_BORDER  0x8C71   // darker soft gray border

#define ROW_RED         0xD986
#define ROW_ORANGE      0xE4E0
#define ROW_YELLOW      0xEEC0
#define ROW_GREEN       0x5B67
#define ROW_BLUE        0x39CE
#define ROW_PURPLE      0x89D7
#define ROW_PINK        0xD1B2

// ---------------- HELPERS ----------------
uint16_t rowColor(int rowIndex) {
  switch (rowIndex % 7) {
    case 0: return ROW_RED;
    case 1: return ROW_ORANGE;
    case 2: return ROW_YELLOW;
    case 3: return ROW_GREEN;
    case 4: return ROW_BLUE;
    case 5: return ROW_PURPLE;
    default: return ROW_PINK;
  }
}

int rtcDayToMondayIndex(int rtcDow) {
  // RTClib: 0=Sunday, 1=Monday, ... 6=Saturday
  if (rtcDow == 0) return 6;
  return rtcDow - 1;
}

int completedHabitsToday() {
  DateTime now = rtc.now();
  int dayCol = rtcDayToMondayIndex(now.dayOfTheWeek());

  int count = 0;
  for (int i = 0; i < habitCount; i++) {
    if (weeklyDone[i][dayCol]) count++;
  }
  return count;
}

void adjustScroll() {
  if (habitIndex < scrollOffset) {
    scrollOffset = habitIndex;
  } else if (habitIndex >= scrollOffset + visibleHabits) {
    scrollOffset = habitIndex - visibleHabits + 1;
  }

  int maxOffset = habitCount - visibleHabits;
  if (maxOffset < 0) maxOffset = 0;

  if (scrollOffset < 0) scrollOffset = 0;
  if (scrollOffset > maxOffset) scrollOffset = maxOffset;
}

String monthShort(int m) {
  const char* months[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  };
  if (m < 1 || m > 12) return "";
  return months[m - 1];
}

String formatShortDate(DateTime d) {
  return monthShort(d.month()) + " " + String(d.day());
}

String weeklyHeaderText() {
  DateTime now = rtc.now();
  int dayCol = rtcDayToMondayIndex(now.dayOfTheWeek());

  DateTime startOfWeek = now - TimeSpan(dayCol, 0, 0, 0);
  DateTime endOfWeek   = startOfWeek + TimeSpan(6, 0, 0, 0);

  return formatShortDate(startOfWeek) + " - " + formatShortDate(endOfWeek);
}

String shortenHabit(const char* label, int maxLen) {
  String s = String(label);
  if ((int)s.length() <= maxLen) return s;
  return s.substring(0, maxLen - 1) + ".";
}

void drawBoldText(int font, const String& text, int x, int y, uint16_t fg) {
  tft.setTextFont(font);
  tft.setTextColor(fg);

  tft.drawString(text, x, y);
  tft.drawString(text, x, y + 1);
}

// ---------------- ICONS ----------------
void drawHamburgerIcon(int x, int y, uint16_t color) {
  tft.drawFastHLine(x, y, 14, color);
  tft.drawFastHLine(x, y + 4, 14, color);
  tft.drawFastHLine(x, y + 8, 14, color);
}

void drawDropletIcon(int x, int y, uint16_t color) {
  tft.fillCircle(x + 6, y + 8, 4, color);
  tft.fillTriangle(x + 6, y, x + 2, y + 7, x + 10, y + 7, color);
}

void drawBookIcon(int x, int y, uint16_t color) {
  tft.fillRoundRect(x, y + 1, 5, 12, 2, color);
  tft.fillRoundRect(x + 7, y + 1, 5, 12, 2, color);
  tft.drawFastVLine(x + 6, y + 1, 12, UI_BG);
}

void drawDumbbellIcon(int x, int y, uint16_t color) {
  tft.fillRect(x + 3, y + 5, 8, 3, color);
  tft.fillRect(x, y + 2, 2, 9, color);
  tft.fillRect(x + 12, y + 2, 2, 9, color);
  tft.fillRect(x + 2, y + 3, 1, 7, color);
  tft.fillRect(x + 11, y + 3, 1, 7, color);
}

void drawArrowIcon(int x, int y, uint16_t color) {
  tft.fillRect(x, y + 5, 8, 3, color);
  tft.fillTriangle(x + 8, y + 1, x + 8, y + 12, x + 14, y + 6, color);
}

void drawLeafIcon(int x, int y, uint16_t color) {
  tft.fillEllipse(x + 6, y + 6, 5, 3, color);
  tft.drawLine(x + 3, y + 10, x + 8, y + 3, UI_BG);
}

void drawMoonIcon(int x, int y, uint16_t color) {
  tft.fillCircle(x + 6, y + 6, 5, color);
  tft.fillCircle(x + 8, y + 5, 4, UI_BG);
}

void drawStretchIcon(int x, int y, uint16_t color) {
  tft.fillCircle(x + 6, y + 2, 2, color);
  tft.drawLine(x + 6, y + 4, x + 6, y + 10, color);
  tft.drawLine(x + 6, y + 5, x + 2, y + 7, color);
  tft.drawLine(x + 6, y + 5, x + 10, y + 7, color);
  tft.drawLine(x + 6, y + 10, x + 3, y + 13, color);
  tft.drawLine(x + 6, y + 10, x + 9, y + 13, color);
}

void drawHabitIcon(int iconIndex, int x, int y, uint16_t color) {
  switch (iconIndex % 7) {
    case 0: drawDropletIcon(x, y, color); break;
    case 1: drawBookIcon(x, y, color); break;
    case 2: drawDumbbellIcon(x, y, color); break;
    case 3: drawArrowIcon(x, y, color); break;
    case 4: drawLeafIcon(x, y, color); break;
    case 5: drawMoonIcon(x, y, color); break;
    default: drawStretchIcon(x, y, color); break;
  }
}

// ---------------- PROGRESS BAR ----------------
void drawProgressBar(int x, int y, int w, int h, int value, int maxValue) {
  tft.drawRoundRect(x, y, w, h, 4, UI_BLUE);

  int fillW = 0;
  if (maxValue > 0) {
    fillW = ((w - 4) * value) / maxValue;
  }

  if (fillW > 0) {
    tft.fillRoundRect(x + 2, y + 2, fillW, h - 4, 3, UI_BLUE);
  }
}

// ---------------- UI DRAWING ----------------
void drawTopBar(const char* title) {
  tft.fillRect(0, 0, 320, 34, UI_BG);
  tft.drawFastHLine(0, 34, 320, UI_BORDER);

  drawHamburgerIcon(12, 13, UI_MIDGRAY);

  tft.setTextFont(4);
  tft.setTextColor(UI_BLACK, UI_BG);
  tft.drawString(title, 36, 6);

  int done = completedHabitsToday();
  drawProgressBar(176, 10, 86, 12, done, habitCount);

  tft.fillRect(264, 8, 42, 18, UI_BG);
  drawBoldText(2, String(done) + "/" + String(habitCount), 267, 10, UI_DARKBLUE);
}

void drawHabitCard(int y, const char* label, int iconIndex, bool selected, bool doneToday, uint16_t accentColor) {
  uint16_t fillColor = UI_CARD;
  uint16_t borderColor = selected ? UI_BROWN : UI_BORDER;

  tft.fillRoundRect(12, y, 296, 40, 8, fillColor);
  tft.drawRoundRect(12, y, 296, 40, 8, borderColor);

  drawHabitIcon(iconIndex, 23, y + 12, accentColor);

  tft.setTextFont(2);
  tft.setTextColor(UI_BLACK, fillColor);
  tft.drawString(label, 48, y + 12);

  if (doneToday) {
    tft.fillRoundRect(279, y + 11, 16, 16, 4, accentColor);
    tft.drawRoundRect(279, y + 11, 16, 16, 4, accentColor);
  } else {
    tft.drawRoundRect(279, y + 11, 16, 16, 4, accentColor);
  }
}

void drawScrollIndicator() {
  if (habitCount <= visibleHabits) return;

  int trackX = 313;
  int trackY = 48;
  int trackH = 180;

  tft.drawRoundRect(trackX, trackY, 4, trackH, 2, UI_LIGHTGRAY);

  int thumbH = (trackH * visibleHabits) / habitCount;
  if (thumbH < 18) thumbH = 18;

  int maxOffset = habitCount - visibleHabits;
  if (maxOffset < 1) maxOffset = 1;

  int thumbY = trackY + ((trackH - thumbH) * scrollOffset) / maxOffset;
  tft.fillRoundRect(trackX, thumbY, 4, thumbH, 2, UI_BLUE);
}

void drawHabitsPage() {
  tft.fillScreen(UI_BG);
  drawTopBar("Habits");

  DateTime now = rtc.now();
  int todayCol = rtcDayToMondayIndex(now.dayOfTheWeek());

  int startY = 46;
  int rowGap = 8;

  for (int row = 0; row < visibleHabits; row++) {
    int habitPos = scrollOffset + row;
    if (habitPos >= habitCount) break;

    int y = startY + row * (40 + rowGap);
    drawHabitCard(
      y,
      habits[habitPos],
      habitPos,
      (habitPos == habitIndex && !menuOpen),
      weeklyDone[habitPos][todayCol],
      rowColor(habitPos)
    );
  }

  drawScrollIndicator();
}

void drawStatsHeader() {
  const char* days[7] = {"M", "T", "W", "Th", "F", "Sa", "Su"};
  int dayX[7] = {150, 172, 194, 216, 238, 260, 282};

  DateTime now = rtc.now();
  int todayCol = rtcDayToMondayIndex(now.dayOfTheWeek());
  int todayColX[7] = {143, 165, 187, 209, 231, 253, 275};

  // light highlight behind today's whole column
  tft.fillRoundRect(todayColX[todayCol] - 3, 52, 20, 172, 6, UI_TODAY_COL);

  String dateText = weeklyHeaderText();

  // clear date area
  tft.fillRect(8, 48, 136, 40, UI_BG);

  // flat warm fill behind the date, extending closer to the side borders
  tft.fillRect(15, 52, 118, 32, UI_TAPE_FILL);

  // LEFT border piece - stretched upward only
  tft.drawFastVLine(12, 51, 34, UI_BROWN);
  tft.drawFastVLine(13, 51, 34, UI_BROWN);
  tft.drawFastVLine(14, 53, 30, UI_BROWN);

  tft.drawFastHLine(12, 51, 26, UI_BROWN);
  tft.drawFastHLine(12, 52, 26, UI_BROWN);

  tft.drawFastHLine(12, 83, 26, UI_BROWN);
  tft.drawFastHLine(12, 84, 26, UI_BROWN);

  // RIGHT border piece - stretched upward only
  tft.drawFastVLine(134, 51, 34, UI_BROWN);
  tft.drawFastVLine(133, 51, 34, UI_BROWN);
  tft.drawFastVLine(132, 53, 30, UI_BROWN);

  tft.drawFastHLine(108, 51, 26, UI_BROWN);
  tft.drawFastHLine(108, 52, 26, UI_BROWN);

  tft.drawFastHLine(108, 83, 26, UI_BROWN);
  tft.drawFastHLine(108, 84, 26, UI_BROWN);

  drawBoldText(2, dateText, 22, 60, UI_BLACK);

  tft.setTextFont(2);
  tft.setTextColor(UI_DARKGRAY, UI_BG);

  for (int i = 0; i < 7; i++) {
    if (i == todayCol) {
      tft.setTextColor(UI_DARKBLUE, UI_TODAY_COL);
    } else {
      tft.setTextColor(UI_DARKGRAY, UI_BG);
    }
    tft.drawCentreString(days[i], dayX[i], 60, 2);
  }

  tft.drawFastHLine(146, 84, 150, UI_LIGHTGRAY);
}

void drawWeeklyCell(int x, int y, bool filled, bool isToday, uint16_t accentColor) {
  if (filled) {
    tft.fillRoundRect(x, y, 14, 14, 4, accentColor);
    tft.drawRoundRect(x, y, 14, 14, 4, accentColor);
  } else {
    tft.fillRoundRect(x, y, 14, 14, 4, UI_BG);
    tft.drawRoundRect(x, y, 14, 14, 4, accentColor);
  }
}

void drawStatsRow(int rowY, int habitIdx, bool selected, int todayCol) {
  if (selected) {
    tft.drawRoundRect(10, rowY - 2, 298, 28, 8, UI_BROWN);
  }

  uint16_t accent = rowColor(habitIdx);

  tft.setTextFont(2);
  tft.setTextColor(UI_BLACK, UI_BG);
  tft.drawString(shortenHabit(habits[habitIdx], 11), 16, rowY + 6);

  int cellX[7] = {143, 165, 187, 209, 231, 253, 275};
  for (int d = 0; d < 7; d++) {
    drawWeeklyCell(cellX[d], rowY + 5, weeklyDone[habitIdx][d], d == todayCol, accent);
  }

  tft.drawFastHLine(14, rowY + 28, 290, UI_LIGHTGRAY);
}

void drawStatsPage() {
  tft.fillScreen(UI_BG);
  drawTopBar("Stats");
  drawStatsHeader();

  DateTime now = rtc.now();
  int todayCol = rtcDayToMondayIndex(now.dayOfTheWeek());

  const int statsVisible = 4;
  int statsStart = 0;

  if (habitIndex >= statsVisible) {
    statsStart = habitIndex - statsVisible + 1;
  }
  if (statsStart > habitCount - statsVisible) {
    statsStart = max(0, habitCount - statsVisible);
  }

  int startY = 96;
  int rowStep = 32;

  for (int row = 0; row < statsVisible; row++) {
    int h = statsStart + row;
    if (h >= habitCount) break;

    int y = startY + row * rowStep;
    bool selected = (h == habitIndex && !menuOpen);
    drawStatsRow(y, h, selected, todayCol);
  }
}

void drawSettingsPage() {
  tft.fillScreen(UI_BG);
  drawTopBar("Settings");

  tft.fillRoundRect(18, 56, 284, 46, 10, UI_CARD);
  tft.drawRoundRect(18, 56, 284, 46, 10, UI_BORDER);
  tft.setTextFont(2);
  tft.setTextColor(UI_BLACK, UI_CARD);
  tft.drawString("Button Mode", 28, 66);
  tft.setTextColor(UI_DARKGRAY, UI_CARD);
  tft.drawString("Controlled by physical buttons", 28, 84);

  tft.fillRoundRect(18, 112, 284, 46, 10, UI_CARD);
  tft.drawRoundRect(18, 112, 284, 46, 10, UI_BORDER);
  tft.setTextColor(UI_BLACK, UI_CARD);
  tft.drawString("RTC Clock", 28, 122);
  tft.setTextColor(UI_DARKGRAY, UI_CARD);
  tft.drawString("Used for weekly date tracking", 28, 140);
}

void drawSidebar() {
  tft.fillRect(0, 0, 136, 240, UI_SIDEBAR);
  tft.drawFastVLine(135, 0, 240, UI_BORDER);

  drawHamburgerIcon(12, 13, UI_MIDGRAY);

  tft.setTextFont(4);
  tft.setTextColor(UI_BLACK, UI_SIDEBAR);
  tft.drawString("Menu", 36, 6);

  for (int i = 0; i < 3; i++) {
    int y = 54 + i * 32;
    uint16_t fill = (i == menuIndex) ? UI_BLUE : UI_SIDEBAR;
    uint16_t text = (i == menuIndex) ? UI_BG : UI_BLACK;

    if (i == menuIndex) {
      tft.fillRoundRect(10, y, 114, 22, 6, fill);
    }

    tft.setTextFont(2);
    tft.setTextColor(text, fill);
    tft.drawString(menuItems[i], 20, y + 5);
  }
}

void drawScreen() {
  if (currentPage == PAGE_HABITS) drawHabitsPage();
  else if (currentPage == PAGE_STATS) drawStatsPage();
  else drawSettingsPage();

  if (menuOpen) {
    drawSidebar();
  }
}

// ---------------- BUTTON LOGIC ----------------
bool pressed(int pin, bool &lastState) {
  bool current = digitalRead(pin);

  if (lastState == HIGH && current == LOW && millis() - lastPress > debounceMs) {
    lastPress = millis();
    lastState = current;
    return true;
  }

  lastState = current;
  return false;
}

void toggleTodayForSelectedHabit() {
  DateTime now = rtc.now();
  int dayCol = rtcDayToMondayIndex(now.dayOfTheWeek());
  weeklyDone[habitIndex][dayCol] = !weeklyDone[habitIndex][dayCol];
}

void handleMenuPress() {
  menuOpen = !menuOpen;

  if (menuOpen) {
    if (currentPage == PAGE_HABITS) menuIndex = 0;
    else if (currentPage == PAGE_STATS) menuIndex = 1;
    else menuIndex = 2;
  }

  drawScreen();
}

void handleNextPress() {
  if (menuOpen) {
    menuIndex++;
    if (menuIndex >= 3) {
      menuIndex = 0;
    }
  } else {
    habitIndex++;
    if (habitIndex >= habitCount) {
      habitIndex = 0;
      scrollOffset = 0;
    } else {
      adjustScroll();
    }
  }

  drawScreen();
}

void handleSelectPress() {
  if (menuOpen) {
    if (menuIndex == 0) currentPage = PAGE_HABITS;
    else if (menuIndex == 1) currentPage = PAGE_STATS;
    else currentPage = PAGE_SETTINGS;

    menuOpen = false;
  } else if (currentPage == PAGE_HABITS || currentPage == PAGE_STATS) {
    toggleTodayForSelectedHabit();
  }

  drawScreen();
}

// ---------------- SETUP ----------------
void setup() {
  pinMode(MENU_BTN, INPUT_PULLUP);
  pinMode(NEXT_BTN, INPUT_PULLUP);
  pinMode(SELECT_BTN, INPUT_PULLUP);

  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);

  Wire.begin(RTC_SDA, RTC_SCL);

  rtc.begin();

  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // Uncomment once if the RTC date/time is wrong, upload, then comment it again and upload again.
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(UI_BG);

  adjustScroll();
  drawScreen();
}

// ---------------- LOOP ----------------
void loop() {
  if (pressed(MENU_BTN, lastMenu)) handleMenuPress();
  if (pressed(NEXT_BTN, lastNext)) handleNextPress();
  if (pressed(SELECT_BTN, lastSelect)) handleSelectPress();
}