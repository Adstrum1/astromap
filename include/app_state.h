#pragma once
#include <Adafruit_SSD1306.h>

enum AppState { STATE_MENU, STATE_TIME_SET, STATE_MAP, STATE_COORDINATES_SET };
extern AppState state;

extern Adafruit_SSD1306 display;
