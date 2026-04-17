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

// ---------------- SETTINGS UI STATE ----------------
int settingsMainIndex = 0;      // 0 = Set Date, 1 = Set Time, 2 = Pomodoro
bool dateEditorOpen = false;
bool timeEditorOpen = false;
bool settingsDropdownOpen = false;

// Date editor
int dateFieldIndex = 0;         // 0 month, 1 date, 2 year, 3 done
int dateDropdownIndex = 0;
int editMonth = 1;
int editDay = 1;
int editYear = 2026;

// Time editor
int timeFieldIndex = 0;         // 0 hour, 1 minute, 2 am/pm, 3 done
int timeDropdownIndex = 0;
int editHour12 = 12;
int editMinute = 0;             // 5-minute increments
int editAmPm = 0;               // 0=AM, 1=PM

const int minuteOptionCount = 60;

// ---------------- BUTTON STATE ----------------
bool lastMenu = HIGH;
bool lastNext = HIGH;
bool lastSelect = HIGH;

unsigned long lastPress = 0;
const unsigned long debounceMs = 160;

// ---------------- POMODORO UI STATE ----------------
bool pomodoroEditorOpen = false;
bool pomodoroValueEditOpen = false;

int pomodoroFieldIndex = 0;   // 0=sections, 1=time / section, 2=break, 3=start
int pomodoroValueIndex = 0;

// User inputs
int pomodoroSections = 3;
int pomodoroStudyMin = 15;
int pomodoroBreakMin = 5;

// Running state
bool pomodoroRunning = false;
bool pomodoroFinished = false;

enum PomodoroPhase {
  POMO_STUDY,
  POMO_BREAK
};

PomodoroPhase pomodoroPhase = POMO_STUDY;
int pomodoroCurrentSection = 1;           // 1..pomodoroSections
unsigned long pomodoroPhaseStartMs = 0;
unsigned long pomodoroPhaseDurationSec = 0;
int lastDrawnRemainingSec = -1;

// ---------------- COLORS ----------------
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
#define UI_TODAY_COL    0xFF59   // light highlight
#define UI_LIGHTGRAY    0xDEDB
#define UI_MIDGRAY      0x94B2
#define UI_DARKGRAY     0x632C
#define UI_SIDEBAR      0xF6F7   // pale warm gray
#define UI_DATE_BG      0xEF3C
#define UI_DATE_BORDER  0x8C71

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

String monthLong(int m) {
  const char* months[] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
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

bool isLeapYear(int year) {
  if (year % 400 == 0) return true;
  if (year % 100 == 0) return false;
  return (year % 4 == 0);
}

int daysInMonth(int month, int year) {
  switch (month) {
    case 1: return 31;
    case 2: return isLeapYear(year) ? 29 : 28;
    case 3: return 31;
    case 4: return 30;
    case 5: return 31;
    case 6: return 30;
    case 7: return 31;
    case 8: return 31;
    case 9: return 30;
    case 10: return 31;
    case 11: return 30;
    default: return 31;
  }
}

String twoDigit(int v) {
  if (v < 10) return "0" + String(v);
  return String(v);
}

// ---------------- SETTINGS HELPERS ----------------
void openDateEditor() {
  DateTime now = rtc.now();
  editMonth = now.month();
  editDay = now.day();
  editYear = now.year();

  dateFieldIndex = 0;
  dateDropdownIndex = 0;
  dateEditorOpen = true;
  timeEditorOpen = false;
  settingsDropdownOpen = false;
}

void openTimeEditor() {
  DateTime now = rtc.now();
  int h24 = now.hour();

  editAmPm = (h24 >= 12) ? 1 : 0;
  editHour12 = h24 % 12;
  if (editHour12 == 0) editHour12 = 12;

  editMinute = now.minute();

  timeFieldIndex = 0;
  timeDropdownIndex = 0;
  timeEditorOpen = true;
  dateEditorOpen = false;
  settingsDropdownOpen = false;
}

void closeSettingsEditors() {
  dateEditorOpen = false;
  timeEditorOpen = false;
  settingsDropdownOpen = false;
}

int dateDropdownCount() {
  if (dateFieldIndex == 0) return 12;
  if (dateFieldIndex == 1) return daysInMonth(editMonth, editYear);
  if (dateFieldIndex == 2) return 16; // 2025..2040
  return 1;
}

int timeDropdownCount() {
  if (timeFieldIndex == 0) return 12;
  if (timeFieldIndex == 1) return minuteOptionCount;
  if (timeFieldIndex == 2) return 2;
  return 1;
}

void openDateDropdownForCurrentField() {
  settingsDropdownOpen = true;
  if (dateFieldIndex == 0) dateDropdownIndex = editMonth - 1;
  else if (dateFieldIndex == 1) dateDropdownIndex = editDay - 1;
  else if (dateFieldIndex == 2) dateDropdownIndex = editYear - 2025;
}

void openTimeDropdownForCurrentField() {
  settingsDropdownOpen = true;
  if (timeFieldIndex == 0) dateDropdownIndex = 0; // unused
  if (timeFieldIndex == 0) timeDropdownIndex = editHour12 - 1;
  else if (timeFieldIndex == 1) {
    timeDropdownIndex = editMinute;
  }
  else if (timeFieldIndex == 2) timeDropdownIndex = editAmPm;
}

void commitDateDropdownSelection() {
  if (dateFieldIndex == 0) {
    editMonth = dateDropdownIndex + 1;
    int maxDay = daysInMonth(editMonth, editYear);
    if (editDay > maxDay) editDay = maxDay;
  } else if (dateFieldIndex == 1) {
    editDay = dateDropdownIndex + 1;
  } else if (dateFieldIndex == 2) {
    editYear = 2025 + dateDropdownIndex;
    int maxDay = daysInMonth(editMonth, editYear);
    if (editDay > maxDay) editDay = maxDay;
  }
  settingsDropdownOpen = false;
}

void commitTimeDropdownSelection() {
  if (timeFieldIndex == 0) {
    editHour12 = timeDropdownIndex + 1;
  } else if (timeFieldIndex == 1) {
    editMinute = timeDropdownIndex;
  } else if (timeFieldIndex == 2) {
    editAmPm = timeDropdownIndex;
  }
  settingsDropdownOpen = false;
}

void applyDateToRTC() {
  DateTime now = rtc.now();
  rtc.adjust(DateTime(editYear, editMonth, editDay, now.hour(), now.minute(), now.second()));
}

void applyTimeToRTC() {
  DateTime now = rtc.now();

  int hour24 = editHour12 % 12;
  if (editAmPm == 1) hour24 += 12;

  rtc.adjust(DateTime(now.year(), now.month(), now.day(), hour24, editMinute, 0));
}

void settingsHandleNext() {
  if (!dateEditorOpen && !timeEditorOpen && !pomodoroEditorOpen) {
    settingsMainIndex++;
    if (settingsMainIndex > 2) settingsMainIndex = 0;
    return;
  }

  if (dateEditorOpen) {
    if (settingsDropdownOpen) {
      dateDropdownIndex++;
      if (dateDropdownIndex >= dateDropdownCount()) dateDropdownIndex = 0;
    } else {
      dateFieldIndex++;
      if (dateFieldIndex > 3) dateFieldIndex = 0;
    }
    return;
  }

  if (timeEditorOpen) {
    if (settingsDropdownOpen) {
      timeDropdownIndex++;
      if (timeDropdownIndex >= timeDropdownCount()) timeDropdownIndex = 0;
    } else {
      timeFieldIndex++;
      if (timeFieldIndex > 3) timeFieldIndex = 0;
    }
    return;
  }

  if (pomodoroEditorOpen) {
    if (pomodoroValueEditOpen) {
      pomodoroValueIndex++;
      if (pomodoroValueIndex >= pomodoroValueCount()) pomodoroValueIndex = 0;
    } else {
      pomodoroFieldIndex++;
      if (pomodoroFieldIndex > 3) pomodoroFieldIndex = 0;
    }
  }
}

void settingsHandleSelect() {
  if (!dateEditorOpen && !timeEditorOpen && !pomodoroEditorOpen) {
    if (settingsMainIndex == 0) openDateEditor();
    else if (settingsMainIndex == 1) openTimeEditor();
    else openPomodoroEditor();
    return;
  }

  if (dateEditorOpen) {
    if (settingsDropdownOpen) {
      commitDateDropdownSelection();
    } else {
      if (dateFieldIndex == 3) {
        applyDateToRTC();
        closeAllSettingsEditors();
      } else {
        openDateDropdownForCurrentField();
      }
    }
    return;
  }

  if (timeEditorOpen) {
    if (settingsDropdownOpen) {
      commitTimeDropdownSelection();
    } else {
      if (timeFieldIndex == 3) {
        applyTimeToRTC();
        closeAllSettingsEditors();
      } else {
        openTimeDropdownForCurrentField();
      }
    }
    return;
  }

  if (pomodoroEditorOpen) {
    if (pomodoroValueEditOpen) {
      commitPomodoroValue();
    } else {
      if (pomodoroFieldIndex == 3) {
        startPomodoro();
      } else {
        openPomodoroValueEditor();
      }
    }
  }
}
void openPomodoroEditor() {
  pomodoroFieldIndex = 0;
  pomodoroValueIndex = 0;
  pomodoroEditorOpen = true;
  pomodoroValueEditOpen = false;

  dateEditorOpen = false;
  timeEditorOpen = false;
  settingsDropdownOpen = false;
}

void closeAllSettingsEditors() {
  dateEditorOpen = false;
  timeEditorOpen = false;
  pomodoroEditorOpen = false;
  pomodoroValueEditOpen = false;
  settingsDropdownOpen = false;
}

int pomodoroValueCount() {
  if (pomodoroFieldIndex == 0) return 12;   // sections: 1..12
  if (pomodoroFieldIndex == 1) return 120;  // study: 1..120 min
  if (pomodoroFieldIndex == 2) return 61;   // break: 0..60 min
  return 1;
}

void openPomodoroValueEditor() {
  pomodoroValueEditOpen = true;

  if (pomodoroFieldIndex == 0) pomodoroValueIndex = pomodoroSections - 1;
  else if (pomodoroFieldIndex == 1) pomodoroValueIndex = pomodoroStudyMin - 1;
  else if (pomodoroFieldIndex == 2) pomodoroValueIndex = pomodoroBreakMin;
}

void commitPomodoroValue() {
  if (pomodoroFieldIndex == 0) pomodoroSections = pomodoroValueIndex + 1;
  else if (pomodoroFieldIndex == 1) pomodoroStudyMin = pomodoroValueIndex + 1;
  else if (pomodoroFieldIndex == 2) pomodoroBreakMin = pomodoroValueIndex;

  pomodoroValueEditOpen = false;
}

String pomodoroValueLabel(int idx) {
  if (pomodoroFieldIndex == 0) return String(idx + 1);
  if (pomodoroFieldIndex == 1) return String(idx + 1);
  return String(idx);
}

void startPomodoro() {
  pomodoroRunning = true;
  pomodoroFinished = false;
  pomodoroPhase = POMO_STUDY;
  pomodoroCurrentSection = 1;
  pomodoroPhaseDurationSec = (unsigned long)pomodoroStudyMin * 60UL;
  pomodoroPhaseStartMs = millis();
  lastDrawnRemainingSec = -1;

  menuOpen = false;
  currentPage = PAGE_SETTINGS;
}

void finishPomodoro() {
  pomodoroRunning = false;
  pomodoroFinished = true;
  drawScreen();
}

void moveToNextPomodoroPhase() {
  if (pomodoroPhase == POMO_STUDY) {
    // final study finished -> done
    if (pomodoroCurrentSection >= pomodoroSections) {
      finishPomodoro();
      return;
    }

    // if break is 0, skip directly to next study section
    if (pomodoroBreakMin == 0) {
      pomodoroCurrentSection++;
      pomodoroPhase = POMO_STUDY;
      pomodoroPhaseDurationSec = (unsigned long)pomodoroStudyMin * 60UL;
      pomodoroPhaseStartMs = millis();
      lastDrawnRemainingSec = -1;
      drawScreen();
      return;
    }

    pomodoroPhase = POMO_BREAK;
    pomodoroPhaseDurationSec = (unsigned long)pomodoroBreakMin * 60UL;
    pomodoroPhaseStartMs = millis();
    lastDrawnRemainingSec = -1;
    drawScreen();
  } else {
    // break finished -> next section study
    pomodoroCurrentSection++;
    pomodoroPhase = POMO_STUDY;
    pomodoroPhaseDurationSec = (unsigned long)pomodoroStudyMin * 60UL;
    pomodoroPhaseStartMs = millis();
    lastDrawnRemainingSec = -1;
    drawScreen();
  }
}

int pomodoroRemainingSeconds() {
  unsigned long elapsedSec = (millis() - pomodoroPhaseStartMs) / 1000UL;

  if (elapsedSec >= pomodoroPhaseDurationSec) return 0;
  return (int)(pomodoroPhaseDurationSec - elapsedSec);
}

String formatTimerMMSS(int totalSec) {
  int mm = totalSec / 60;
  int ss = totalSec % 60;
  return twoDigit(mm) + ":" + twoDigit(ss);
}

void updatePomodoroTimer() {
  if (!pomodoroRunning) return;

  int remain = pomodoroRemainingSeconds();

  if (remain != lastDrawnRemainingSec) {
    lastDrawnRemainingSec = remain;
    drawScreen();
  }

  unsigned long elapsedSec = (millis() - pomodoroPhaseStartMs) / 1000UL;
  if (elapsedSec >= pomodoroPhaseDurationSec) {
    moveToNextPomodoroPhase();
  }
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

  tft.fillRoundRect(todayColX[todayCol] - 3, 52, 20, 172, 6, UI_TODAY_COL);

  String dateText = weeklyHeaderText();

  tft.fillRect(8, 48, 136, 40, UI_BG);

  // date frame
  tft.fillRect(15, 52, 118, 32, UI_TAPE_FILL);

  tft.drawFastVLine(12, 51, 34, UI_BROWN);
  tft.drawFastVLine(13, 51, 34, UI_BROWN);
  tft.drawFastVLine(14, 53, 30, UI_BROWN);

  tft.drawFastHLine(12, 51, 26, UI_BROWN);
  tft.drawFastHLine(12, 52, 26, UI_BROWN);

  tft.drawFastHLine(12, 83, 26, UI_BROWN);
  tft.drawFastHLine(12, 84, 26, UI_BROWN);

  tft.drawFastVLine(134, 51, 34, UI_BROWN);
  tft.drawFastVLine(133, 51, 34, UI_BROWN);
  tft.drawFastVLine(132, 53, 30, UI_BROWN);

  tft.drawFastHLine(108, 51, 26, UI_BROWN);
  tft.drawFastHLine(108, 52, 26, UI_BROWN);

  tft.drawFastHLine(108, 83, 26, UI_BROWN);
  tft.drawFastHLine(108, 84, 26, UI_BROWN);

  drawBoldText(2, dateText, 22, 60, UI_BLACK);

  tft.setTextFont(2);

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

// ---------------- SETTINGS DRAWING ----------------
void drawSettingsButtonCard(int x, int y, int w, int h, const char* label, bool selected) {
  uint16_t border = selected ? UI_BROWN : UI_BORDER;

  tft.fillRoundRect(x, y, w, h, 8, UI_CARD);
  tft.drawRoundRect(x, y, w, h, 8, border);
  if (selected) {
    tft.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 8, border);
  }

  tft.setTextFont(2);
  tft.setTextColor(UI_BLACK, UI_CARD);
  tft.drawCentreString(label, x + w / 2, y + 12, 2);
}

void drawSettingsFieldChip(int x, int y, int w, int h, const String& value, bool selected, bool activeDropdown) {
  uint16_t border = selected ? UI_BROWN : UI_BORDER;
  uint16_t fill = UI_CARD;

  tft.fillRoundRect(x, y, w, h, 6, fill);
  tft.drawRoundRect(x, y, w, h, 6, border);

  if (activeDropdown) {
    tft.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 6, UI_DARKBLUE);
  }

  tft.setTextFont(2);
  tft.setTextColor(UI_BLACK, fill);
  tft.drawCentreString(value, x + w / 2, y + 9, 2);
}

void drawDropdownList(int x, int y, int w, int itemH, int count, int selectedIndex, String (*labelFn)(int)) {
  int visible = (count < 4) ? count : 4;
  int start = selectedIndex - 1;
  if (start < 0) start = 0;
  if (start > count - visible) start = count - visible;
  if (start < 0) start = 0;

  int h = visible * itemH + 8;

  tft.fillRoundRect(x, y, w, h, 8, UI_CARD);
  tft.drawRoundRect(x, y, w, h, 8, UI_BROWN);

  for (int i = 0; i < visible; i++) {
    int idx = start + i;
    int itemY = y + 4 + i * itemH;

    if (idx == selectedIndex) {
      tft.fillRoundRect(x + 4, itemY, w - 8, itemH - 2, 5, UI_SOFTBLUE);
      tft.drawRoundRect(x + 4, itemY, w - 8, itemH - 2, 5, UI_BROWN);
      tft.setTextColor(UI_BLACK, UI_SOFTBLUE);
    } else {
      tft.setTextColor(UI_BLACK, UI_CARD);
    }

    tft.setTextFont(2);
    tft.drawCentreString(labelFn(idx), x + w / 2, itemY + 4, 2);
  }
}

String dateMonthLabel(int idx) { return monthLong(idx + 1); }
String dateDayLabel(int idx) { return String(idx + 1); }
String dateYearLabel(int idx) { return String(2025 + idx); }
String timeHourLabel(int idx) { return String(idx + 1); }
String timeMinuteLabel(int idx) { return twoDigit(idx); }
String timeAmPmLabel(int idx) { return idx == 0 ? "AM" : "PM"; }

void drawDateEditor() {
  tft.setTextFont(2);
  tft.setTextColor(UI_DARKGRAY, UI_BG);
  tft.drawString("Month", 16, 103);
  tft.drawString("Date", 92, 103);
  tft.drawString("Year", 152, 103);
  tft.drawString("Done", 224, 103);

  drawSettingsFieldChip(12, 120, 68, 28, monthLong(editMonth), dateFieldIndex == 0, dateEditorOpen && settingsDropdownOpen && dateFieldIndex == 0);
  drawSettingsFieldChip(88, 120, 52, 28, String(editDay), dateFieldIndex == 1, dateEditorOpen && settingsDropdownOpen && dateFieldIndex == 1);
  drawSettingsFieldChip(148, 120, 68, 28, String(editYear), dateFieldIndex == 2, dateEditorOpen && settingsDropdownOpen && dateFieldIndex == 2);
  drawSettingsFieldChip(224, 120, 68, 28, "Done", dateFieldIndex == 3, false);

  tft.setTextFont(2);
  tft.setTextColor(UI_DARKGRAY, UI_BG);
  tft.drawCentreString("NEXT moves, SELECT opens/chooses", 160, 212, 2);

  if (settingsDropdownOpen) {
    if (dateFieldIndex == 0) {
      drawDropdownList(12, 156, 120, 18, 12, dateDropdownIndex, dateMonthLabel);
    } else if (dateFieldIndex == 1) {
      drawDropdownList(88, 156, 70, 18, daysInMonth(editMonth, editYear), dateDropdownIndex, dateDayLabel);
    } else if (dateFieldIndex == 2) {
      drawDropdownList(148, 156, 82, 18, 16, dateDropdownIndex, dateYearLabel);
    }
  }
}

void drawTimeEditor() {
  tft.setTextFont(2);
  tft.setTextColor(UI_DARKGRAY, UI_BG);
  tft.drawString("Hour", 16, 103);
  tft.drawString("Minute", 92, 103);
  tft.drawString("AM/PM", 172, 103);
  tft.drawString("Done", 244, 103);

  drawSettingsFieldChip(12, 120, 60, 28, String(editHour12), timeFieldIndex == 0, timeEditorOpen && settingsDropdownOpen && timeFieldIndex == 0);
  drawSettingsFieldChip(84, 120, 72, 28, twoDigit(editMinute), timeFieldIndex == 1, timeEditorOpen && settingsDropdownOpen && timeFieldIndex == 1);
  drawSettingsFieldChip(168, 120, 60, 28, editAmPm == 0 ? "AM" : "PM", timeFieldIndex == 2, timeEditorOpen && settingsDropdownOpen && timeFieldIndex == 2);
  drawSettingsFieldChip(240, 120, 52, 28, "Done", timeFieldIndex == 3, false);

  tft.setTextFont(2);
  tft.setTextColor(UI_DARKGRAY, UI_BG);
  tft.drawCentreString("Minutes use 1-minute steps", 160, 194, 2);
  tft.drawCentreString("NEXT moves, SELECT opens/chooses", 160, 212, 2);

  if (settingsDropdownOpen) {
    if (timeFieldIndex == 0) {
      drawDropdownList(12, 156, 70, 18, 12, timeDropdownIndex, timeHourLabel);
    } else if (timeFieldIndex == 1) {
      drawDropdownList(84, 156, 80, 18, minuteOptionCount, timeDropdownIndex, timeMinuteLabel);
    } else if (timeFieldIndex == 2) {
      drawDropdownList(168, 156, 70, 18, 2, timeDropdownIndex, timeAmPmLabel);
    }
  }
}

void drawSettingsPage() {
  if (pomodoroRunning) {
    drawPomodoroRunningPage();
    return;
  }

  if (pomodoroFinished) {
    drawPomodoroFinishedPage();
    return;
  }

  if (pomodoroEditorOpen) {
    drawPomodoroEditor();
    return;
  }

  tft.fillScreen(UI_BG);
  drawTopBar("Settings");

  drawSettingsButtonCard(18, 52, 126, 34, "Set Date", settingsMainIndex == 0);
  drawSettingsButtonCard(176, 52, 126, 34, "Set Time", settingsMainIndex == 1);
  drawSettingsButtonCard(97, 98, 126, 34, "Pomodoro", settingsMainIndex == 2);

  tft.setTextFont(2);
  tft.setTextColor(UI_DARKGRAY, UI_BG);
  tft.drawCentreString("SELECT opens highlighted option", 160, 146, 2);
  tft.drawCentreString("NEXT moves between Date / Time / Pomodoro", 160, 164, 2);

  DateTime now = rtc.now();
  String currentDate = monthLong(now.month()) + " " + String(now.day()) + ", " + String(now.year());

  int h24 = now.hour();
  int ampm = (h24 >= 12) ? 1 : 0;
  int h12 = h24 % 12;
  if (h12 == 0) h12 = 12;
  String currentTime = String(h12) + ":" + twoDigit(now.minute()) + (ampm ? " PM" : " AM");

  tft.fillRoundRect(18, 184, 284, 22, 8, UI_CARD);
  tft.drawRoundRect(18, 184, 284, 22, 8, UI_BORDER);
  tft.setTextFont(2);
  tft.setTextColor(UI_BLACK, UI_CARD);
  tft.drawString("Date: " + currentDate, 28, 189);

  tft.fillRoundRect(18, 212, 284, 22, 8, UI_CARD);
  tft.drawRoundRect(18, 212, 284, 22, 8, UI_BORDER);
  tft.setTextFont(2);
  tft.setTextColor(UI_BLACK, UI_CARD);
  tft.drawString("Time: " + currentTime, 28, 217);
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

// ---------------- POMODORO DRAWING ----------------
void drawPomodoroRow(int y, const char* label, int value, bool selected, bool editing) {
  uint16_t border = selected ? UI_BROWN : UI_BORDER;

  tft.fillRoundRect(14, y, 292, 46, 8, UI_CARD);
  tft.drawRoundRect(14, y, 292, 46, 8, border);

  if (editing) {
    tft.drawRoundRect(16, y + 2, 288, 42, 8, UI_DARKBLUE);
  }

  tft.setTextFont(2);
  tft.setTextColor(UI_DARKGRAY, UI_CARD);
  tft.drawString(label, 26, y + 7);

  tft.setTextColor(UI_BLACK, UI_CARD);
  tft.drawRightString(String(value) + " min", 286, y + 18, 2);
}

void drawPomodoroSectionsRow(int y, bool selected, bool editing) {
  uint16_t border = selected ? UI_BROWN : UI_BORDER;

  tft.fillRoundRect(14, y, 292, 46, 8, UI_CARD);
  tft.drawRoundRect(14, y, 292, 46, 8, border);

  if (editing) {
    tft.drawRoundRect(16, y + 2, 288, 42, 8, UI_DARKBLUE);
  }

  tft.setTextFont(2);
  tft.setTextColor(UI_DARKGRAY, UI_CARD);
  tft.drawString("Sections", 26, y + 7);

  tft.setTextColor(UI_BLACK, UI_CARD);
  tft.drawRightString(String(pomodoroSections), 286, y + 18, 2);
}

void drawPomodoroStartButton(bool selected) {
  int y = 188;  // bottom 25% area
  uint16_t fill = selected ? UI_BLUE : UI_CARD;
  uint16_t border = selected ? UI_BLUE : UI_BORDER;
  uint16_t textColor = selected ? UI_BG : UI_BLACK;

  tft.fillRoundRect(38, y, 244, 34, 10, fill);
  tft.drawRoundRect(38, y, 244, 34, 10, border);

  tft.setTextFont(2);
  tft.setTextColor(textColor, fill);
  tft.drawCentreString("Start", 160, y + 10, 2);
}

void drawPomodoroEditor() {
  tft.fillScreen(UI_BG);
  drawTopBar("Settings");

  // top 75%
  drawPomodoroSectionsRow(50, pomodoroFieldIndex == 0, pomodoroEditorOpen && pomodoroValueEditOpen && pomodoroFieldIndex == 0);
  drawPomodoroRow(98, "Time / section", pomodoroStudyMin, pomodoroFieldIndex == 1, pomodoroEditorOpen && pomodoroValueEditOpen && pomodoroFieldIndex == 1);
  drawPomodoroRow(146, "Break", pomodoroBreakMin, pomodoroFieldIndex == 2, pomodoroEditorOpen && pomodoroValueEditOpen && pomodoroFieldIndex == 2);

  // bottom 25%
  drawPomodoroStartButton(pomodoroFieldIndex == 3 && !pomodoroValueEditOpen);

  tft.setTextFont(2);
  tft.setTextColor(UI_DARKGRAY, UI_BG);
  tft.drawCentreString("NEXT moves, SELECT opens/chooses", 160, 226, 2);

  if (pomodoroValueEditOpen) {
    if (pomodoroFieldIndex == 0) {
      drawDropdownList(180, 52, 90, 18, 12, pomodoroValueIndex, pomodoroValueLabel);
    } else if (pomodoroFieldIndex == 1) {
      drawDropdownList(180, 100, 90, 18, 120, pomodoroValueIndex, pomodoroValueLabel);
    } else if (pomodoroFieldIndex == 2) {
      drawDropdownList(180, 148, 90, 18, 61, pomodoroValueIndex, pomodoroValueLabel);
    }
  }
}

void drawPomodoroRunningPage() {
  tft.fillScreen(UI_BG);

  String title = (pomodoroPhase == POMO_STUDY) ? "Studying" : "Break time";
  String secText = "section: " + String(pomodoroCurrentSection) + "/" + String(pomodoroSections);

  tft.setTextFont(4);
  tft.setTextColor(UI_BLACK, UI_BG);
  tft.drawString(title, 14, 12);

  tft.setTextFont(2);
  tft.setTextColor(UI_DARKBLUE, UI_BG);
  tft.drawRightString(secText, 306, 18, 2);

  int remain = pomodoroRemainingSeconds();
  String timerText = formatTimerMMSS(remain);

  tft.setTextFont(7);
  tft.setTextColor(UI_DARKBLUE, UI_BG);
  tft.drawCentreString(timerText, 160, 88, 7);
}

void drawPomodoroFinishedPage() {
  tft.fillScreen(UI_BG);

  tft.setTextFont(4);
  tft.setTextColor(UI_DARKBLUE, UI_BG);
  tft.drawCentreString("Good Job!!!", 160, 96, 4);

  tft.setTextFont(2);
  tft.setTextColor(UI_DARKGRAY, UI_BG);
  tft.drawCentreString("Pomodoro complete", 160, 130, 2);
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
  if (pomodoroRunning) {
    pomodoroRunning = false;
    pomodoroFinished = false;
    pomodoroEditorOpen = false;
    pomodoroValueEditOpen = false;
    currentPage = PAGE_SETTINGS;
    drawScreen();
    return;
  }

  if (pomodoroFinished) {
    pomodoroFinished = false;
    currentPage = PAGE_SETTINGS;
    drawScreen();
    return;
  }

  menuOpen = !menuOpen;

  if (menuOpen) {
    if (currentPage == PAGE_HABITS) menuIndex = 0;
    else if (currentPage == PAGE_STATS) menuIndex = 1;
    else menuIndex = 2;
  }

  drawScreen();
}

void handleNextPress() {
  if (pomodoroRunning || pomodoroFinished) return;

  if (menuOpen) {
    menuIndex++;
    if (menuIndex >= 3) {
      menuIndex = 0;
    }
  } else if (currentPage == PAGE_SETTINGS) {
    settingsHandleNext();
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
  if (pomodoroRunning || pomodoroFinished) return;

  if (menuOpen) {
    if (menuIndex == 0) currentPage = PAGE_HABITS;
    else if (menuIndex == 1) currentPage = PAGE_STATS;
    else currentPage = PAGE_SETTINGS;

    menuOpen = false;
  } else if (currentPage == PAGE_SETTINGS) {
    settingsHandleSelect();
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

  updatePomodoroTimer();
}