#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "config.h"
#include "app_state.h"
#include "keypad.h"
#include "datetime.h"
#include "menu_screen.h"
#include "time_screen.h"
#include "coords_screen.h"
#include "map_screen.h"

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
AppState state = STATE_MENU;

void setup() {
  Serial.begin(115200);

  keypadInit();

  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("Initialization error SSD1306");
    while (true) delay(1000);
  }
  display.clearDisplay();
  display.display();

  lastTickMs = millis();
}

void loop() {
  tickClock();
  scanKeypad();

  switch (state) {
    case STATE_MENU:            handleMenu();          break;
    case STATE_TIME_SET:        handleTimeSet();       break;
    case STATE_MAP:              handleMap();          break;
    case STATE_COORDINATES_SET: handleCoordinateSet(); break;
  }

  latchKeys();
  delay(15);
}
