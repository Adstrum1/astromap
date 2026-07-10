#include <math.h>
#include "app_state.h"
#include "keypad.h"
#include "astro.h"
#include "coords_screen.h"
#include "config.h"

// Format: SDDD.DDDD (sign, 3 integer digits, 4 fractional digits).
struct CoordDigits {
  int sign;
  int intPart[3];
  int fracPart[4];
};

static CoordDigits latDigits;
static CoordDigits lonDigits;
static CoordDigits backupLat, backupLon;

static int coordinatesCursor = 0;
static int coordDigitCursor = 0;

static void doubleToDigits(double val, CoordDigits &d) {
  d.sign = (val < 0) ? 1 : 0;
  double av = fabs(val);
  int ip = (int)av;
  double fracD = av - ip;
  int fp = (int)round(fracD * 10000.0);
  if (fp >= 10000) { fp -= 10000; ip += 1; }
  if (ip > 999) ip = 999;

  d.intPart[0] = (ip / 100) % 10;
  d.intPart[1] = (ip / 10) % 10;
  d.intPart[2] = ip % 10;

  d.fracPart[0] = (fp / 1000) % 10;
  d.fracPart[1] = (fp / 100) % 10;
  d.fracPart[2] = (fp / 10) % 10;
  d.fracPart[3] = fp % 10;
}

static double digitsToDouble(const CoordDigits &d) {
  int ip = d.intPart[0]*100 + d.intPart[1]*10 + d.intPart[2];
  int fp = d.fracPart[0]*1000 + d.fracPart[1]*100 + d.fracPart[2]*10 + d.fracPart[3];
  double val = ip + fp / 10000.0;
  return d.sign ? -val : val;
}

void enterCoordinatesScreen() {
  doubleToDigits(OBS_LAT, latDigits);
  doubleToDigits(OBS_LON, lonDigits);
  backupLat = latDigits;
  backupLon = lonDigits;
  coordinatesCursor = 0;
  coordDigitCursor = 0;
}

void drawCoordinateSet() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Coordinates settings");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  CoordDigits *rows[2] = {&latDigits, &lonDigits};

  display.setTextSize(2);
  int y[2] = {16, 34};
  int charX[9] = {0, 12, 24, 36, 48, 60, 72, 84, 96};

  for (int row = 0; row < 2; row++) {
    CoordDigits &d = *rows[row];
    display.setCursor(charX[0], y[row]);
    display.print(d.sign ? "-" : "+");
    display.setCursor(charX[1], y[row]);
    display.print(d.intPart[0]);
    display.setCursor(charX[2], y[row]);
    display.print(d.intPart[1]);
    display.setCursor(charX[3], y[row]);
    display.print(d.intPart[2]);
    display.setCursor(charX[4], y[row]);
    display.print(". ");
    display.setCursor(charX[5], y[row]);
    display.print(d.fracPart[0]);
    display.setCursor(charX[6], y[row]);
    display.print(d.fracPart[1]);
    display.setCursor(charX[7], y[row]);
    display.print(d.fracPart[2]);
    display.setCursor(charX[8], y[row]);
    display.print(d.fracPart[3]);
  }

  int digitToCharIndex[8] = {0, 1, 2, 3, 5, 6, 7, 8};
  int activeCharIdx = digitToCharIndex[coordDigitCursor];
  int ux = charX[activeCharIdx];
  int uy = y[coordinatesCursor];
  display.drawLine(ux, uy + 16, ux + 11, uy + 16, SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 58);
  display.print("^/v:value </> :digit OK:field/save BACK:cancel");
  display.display();
}

static int* getActiveDigitPtr(CoordDigits &d) {
  switch (coordDigitCursor) {
    case 0: return nullptr;
    case 1: return &d.intPart[0];
    case 2: return &d.intPart[1];
    case 3: return &d.intPart[2];
    case 4: return &d.fracPart[0];
    case 5: return &d.fracPart[1];
    case 6: return &d.fracPart[2];
    case 7: return &d.fracPart[3];
  }
  return nullptr;
}

static void changeCoordinatesField(int delta) {
  CoordDigits &d = (coordinatesCursor == 0) ? latDigits : lonDigits;

  if (coordDigitCursor == 0) {
    d.sign = d.sign ? 0 : 1;
  } else {
    int *digit = getActiveDigitPtr(d);
    if (digit) {
      *digit += delta;
      if (*digit < 0) *digit = 9;
      if (*digit > 9) *digit = 0;
    }
  }

  if (coordinatesCursor == 0) {
    double v = digitsToDouble(d);
    if (v < -90.0) v = -90.0;
    if (v > 90.0)  v = 90.0;
    OBS_LAT = v;
    doubleToDigits(OBS_LAT, latDigits);
  } else {
    double v = digitsToDouble(d);
    if (v < -180.0) v = -180.0;
    if (v > 180.0)  v = 180.0;
    OBS_LON = v;
    doubleToDigits(OBS_LON, lonDigits);
  }
}

void handleCoordinateSet() {
  if (keyPressedEdge(KEY_UP))   changeCoordinatesField(+1);
  if (keyPressedEdge(KEY_DOWN)) changeCoordinatesField(-1);
  if (keyPressedEdge(KEY_LEFT)) {
    if (coordDigitCursor > 0) {
      coordDigitCursor--;
    } else if (coordinatesCursor > 0) {
      coordinatesCursor--;
      coordDigitCursor = 7;
    }
  }
  if (keyPressedEdge(KEY_RIGHT)) {
    if (coordDigitCursor < 7) {
      coordDigitCursor++;
    } else if (coordinatesCursor < 1) {
      coordinatesCursor++;
      coordDigitCursor = 0;
    }
  }

  if (keyPressedEdge(KEY_OK)) {
    state = STATE_MENU;
  }
  if (keyPressedEdge(KEY_BACK)) {
    OBS_LAT = digitsToDouble(backupLat);
    OBS_LON = digitsToDouble(backupLon);
    state = STATE_MENU;
  }

  drawCoordinateSet();
}
