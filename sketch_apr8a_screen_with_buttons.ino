#include <TFT_eSPI.h>
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI();

// -------- BUTTON PINS --------
#define MENU_BTN    19
#define NEXT_BTN    18
#define SELECT_BTN  5

// -------- STATE --------
enum Page {
  HABITS,
  STATS,
  SETTINGS
};

Page currentPage = HABITS;

bool menuOpen = false;
int menuIndex = 0;
int habitIndex = 0;

const char* menuItems[3] = {"Habits", "Stats", "Settings"};

const int habitCount = 5;
const char* habits[habitCount] = {
  "Workout",
  "Drink Water",
  "Read",
  "Meditate",
  "Sleep Early"
};

bool habitDone[habitCount] = {false, false, false, false, false};

// -------- BUTTON STATE --------
bool lastMenu = HIGH;
bool lastNext = HIGH;
bool lastSelect = HIGH;

unsigned long lastPress = 0;
const int debounce = 150;

// -------- DRAW FUNCTIONS --------
void drawTopBar(const char* title) {
  tft.fillRect(0, 0, 320, 35, TFT_DARKCYAN);
  tft.setTextColor(TFT_WHITE, TFT_DARKCYAN);
  tft.setTextSize(2);
  tft.drawString(title, 10, 8);
}

void drawHabits() {
  tft.fillScreen(TFT_BLACK);
  drawTopBar("Habits");

  tft.setTextSize(2);

  for (int i = 0; i < habitCount; i++) {
    int y = 50 + i * 32;

    if (i == habitIndex) {
      tft.fillRoundRect(10, y - 2, 300, 28, 6, TFT_BLUE);
      tft.setTextColor(TFT_WHITE, TFT_BLUE);
    } else {
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
    }

    String mark = habitDone[i] ? "[x] " : "[ ] ";
    tft.drawString(mark + String(habits[i]), 18, y);
  }
}

void drawStats() {
  tft.fillScreen(TFT_BLACK);
  drawTopBar("Stats");

  int done = 0;
  for (int i = 0; i < habitCount; i++) {
    if (habitDone[i]) done++;
  }

  int percent = (done * 100) / habitCount;

  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  tft.drawString("Completed:", 20, 70);
  tft.drawString(String(done) + "/" + String(habitCount), 180, 70);

  tft.drawString("Progress:", 20, 110);
  tft.drawString(String(percent) + "%", 180, 110);
}

void drawSettings() {
  tft.fillScreen(TFT_BLACK);
  drawTopBar("Settings");

  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  tft.drawString("Settings Page", 20, 80);
}

void drawMenu() {
  tft.fillRoundRect(40, 50, 240, 130, 8, TFT_DARKGREY);

  tft.setTextSize(2);

  for (int i = 0; i < 3; i++) {
    int y = 65 + i * 35;

    if (i == menuIndex) {
      tft.fillRoundRect(50, y - 2, 220, 28, 6, TFT_BLUE);
      tft.setTextColor(TFT_WHITE, TFT_BLUE);
    } else {
      tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    }

    tft.drawString(menuItems[i], 60, y);
  }
}

void drawScreen() {
  if (currentPage == HABITS) drawHabits();
  else if (currentPage == STATS) drawStats();
  else drawSettings();

  if (menuOpen) drawMenu();
}

// -------- BUTTON LOGIC --------
bool pressed(int pin, bool &lastState) {
  bool current = digitalRead(pin);

  if (lastState == HIGH && current == LOW && millis() - lastPress > debounce) {
    lastPress = millis();
    lastState = current;
    return true;
  }

  lastState = current;
  return false;
}

void handleMenu() {
  menuOpen = !menuOpen;

  if (menuOpen) {
    if (currentPage == HABITS) menuIndex = 0;
    else if (currentPage == STATS) menuIndex = 1;
    else menuIndex = 2;
  }

  drawScreen();
}

void handleNext() {
  if (menuOpen) {
    menuIndex++;
    if (menuIndex > 2) menuIndex = 0;
  } else if (currentPage == HABITS) {
    habitIndex++;
    if (habitIndex >= habitCount) habitIndex = 0;
  }

  drawScreen();
}

void handleSelect() {
  if (menuOpen) {
    if (menuIndex == 0) currentPage = HABITS;
    else if (menuIndex == 1) currentPage = STATS;
    else currentPage = SETTINGS;

    menuOpen = false;
  } else if (currentPage == HABITS) {
    habitDone[habitIndex] = !habitDone[habitIndex];
  }

  drawScreen();
}

// -------- SETUP --------
void setup() {
  pinMode(MENU_BTN, INPUT_PULLUP);
  pinMode(NEXT_BTN, INPUT_PULLUP);
  pinMode(SELECT_BTN, INPUT_PULLUP);

  tft.init();
  tft.setRotation(1);

  drawScreen();
}

// -------- LOOP --------
void loop() {
  if (pressed(MENU_BTN, lastMenu)) handleMenu();
  if (pressed(NEXT_BTN, lastNext)) handleNext();
  if (pressed(SELECT_BTN, lastSelect)) handleSelect();
}