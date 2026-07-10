#include "app_state.h"
#include "keypad.h"
#include "datetime.h"
#include "time_screen.h"
#include "config.h"

// 0=year,1=month,2=day,3=hour,4=min,5=sec

static int timeCursor = 0; 

void enterTimeScreen() {
  clockBackup = clockTime;
  timeCursor = 0;
}

void drawTimeSet() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Time settings");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  char buf[6][6];
  snprintf(buf[0], 6, "%04d", clockTime.year);
  snprintf(buf[1], 6, "%02d", clockTime.month);
  snprintf(buf[2], 6, "%02d", clockTime.day);
  snprintf(buf[3], 6, "%02d", clockTime.hour);
  snprintf(buf[4], 6, "%02d", clockTime.minute);
  snprintf(buf[5], 6, "%02d", clockTime.second);

  display.setTextSize(2);
  int y = 16;
  display.setCursor(0, y);
  display.print(buf[0]); display.print("-"); display.print(buf[1]); display.print("-"); display.print(buf[2]);
  display.setCursor(0, y + 18);
  display.print(buf[3]); display.print(":"); display.print(buf[4]); display.print(":"); display.print(buf[5]);

  int fieldX[6] = {0, 60, 84, 0, 36, 72};
  int fieldW[6] = {48, 24, 24, 24, 24, 24};
  int fieldY[6] = {y, y, y, y + 18, y + 18, y + 18};
  display.drawLine(fieldX[timeCursor], fieldY[timeCursor] + 16,
                    fieldX[timeCursor] + fieldW[timeCursor], fieldY[timeCursor] + 16, SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 56);
  display.print("</> Field ^/v value. OK-save BACK-cancel");
  display.display();
}

static void changeTimeField(int delta) {
  switch (timeCursor) {
    case 0: clockTime.year += delta; if (clockTime.year < 2000) clockTime.year = 2099; if (clockTime.year > 2099) clockTime.year = 2000; break;
    case 1: clockTime.month += delta; if (clockTime.month < 1) clockTime.month = 12; if (clockTime.month > 12) clockTime.month = 1; break;
    case 2: { int dim = daysInMonth(clockTime.month, clockTime.year);
              clockTime.day += delta; if (clockTime.day < 1) clockTime.day = dim; if (clockTime.day > dim) clockTime.day = 1; break; }
    case 3: clockTime.hour += delta; if (clockTime.hour < 0) clockTime.hour = 23; if (clockTime.hour > 23) clockTime.hour = 0; break;
    case 4: clockTime.minute += delta; if (clockTime.minute < 0) clockTime.minute = 59; if (clockTime.minute > 59) clockTime.minute = 0; break;
    case 5: clockTime.second += delta; if (clockTime.second < 0) clockTime.second = 59; if (clockTime.second > 59) clockTime.second = 0; break;
  }
}

void handleTimeSet() {
  if (keyPressedEdge(KEY_UP))    changeTimeField(+1);
  if (keyPressedEdge(KEY_DOWN))  changeTimeField(-1);
  if (keyPressedEdge(KEY_LEFT))  { if (timeCursor > 0) timeCursor--; }
  if (keyPressedEdge(KEY_RIGHT)) { if (timeCursor < 5) timeCursor++; }
  if (keyPressedEdge(KEY_OK)) {
    lastTickMs = millis();
    state = STATE_MENU;
  }
  if (keyPressedEdge(KEY_BACK)) {
    clockTime = clockBackup;
    state = STATE_MENU;
  }
  drawTimeSet();
}
