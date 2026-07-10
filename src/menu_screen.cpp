#include "app_state.h"
#include "config.h"
#include "keypad.h"
#include "datetime.h"
#include "menu_screen.h"
#include "time_screen.h"
#include "coords_screen.h"

static const char* menuItems[] = {"Start", "Time", "Coords"};
static const int menuCount = 3;
static int menuIndex = 0;

void drawMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("MENU  ");
  char buf[9];
  snprintf(buf, 9, "%02d:%02d:%02d", clockTime.hour, clockTime.minute, clockTime.second);
  display.print(buf);
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  const int ITEM_H    = 20;
  const int GLYPH_H   = 12;
  const int LIST_TOP  = 18;
  const int LIST_BOTTOM = SCREEN_HEIGHT;

  int selY = LIST_TOP + menuIndex * ITEM_H;

  int scrollOffset = 0;
  if (selY < LIST_TOP) {
    scrollOffset = selY - LIST_TOP;
  }
  if (selY + GLYPH_H > LIST_BOTTOM) {
    scrollOffset = (selY + GLYPH_H) - LIST_BOTTOM;
  }

  display.setTextSize(2);
  for (int i = 0; i < menuCount; i++) {
    int y = LIST_TOP + i * ITEM_H - scrollOffset;
    if (y + GLYPH_H < 0 || y > SCREEN_HEIGHT) continue;

    display.setCursor(10, y);
    display.print(i == menuIndex ? "> " : "  ");
    display.print(menuItems[i]);
  }
  display.display();
}

void handleMenu() {
  if (keyPressedEdge(KEY_UP))   menuIndex = (menuIndex - 1 + menuCount) % menuCount;
  if (keyPressedEdge(KEY_DOWN)) menuIndex = (menuIndex + 1) % menuCount;
  if (keyPressedEdge(KEY_OK)) {
    if (menuIndex == 0) {
      state = STATE_MAP;
    } else if (menuIndex == 1) {
      enterTimeScreen();
      state = STATE_TIME_SET;
    } else if (menuIndex == 2) {
      enterCoordinatesScreen();
      state = STATE_COORDINATES_SET;
    }
  }
  drawMenu();
}
