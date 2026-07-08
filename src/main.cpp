#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_SDA      21
#define OLED_SCL      22
#define OLED_ADDR     0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

const uint8_t rowPins[4] = {13, 14, 27, 26}; // 1R,2R,3R,4R (OUTPUT)
const uint8_t colPins[4] = {25, 33, 32, 4};  // 1C,2C,3C,4C (INPUT_PULLUP)

bool keyRaw[16];
bool keyStable[16];
unsigned long keyChangeMs[16];
const unsigned long KEY_DEBOUNCE_MS = 30;

#define KEY_UP       0
#define KEY_DOWN     1
#define KEY_LEFT     2
#define KEY_RIGHT    3
#define KEY_ZOOM_IN  4
#define KEY_ZOOM_OUT 5
#define KEY_BACK     6
#define KEY_OK       7


void scanKeypad() {
  for (int r = 0; r < 4; r++) {

    for (int rr = 0; rr < 4; rr++) {
      pinMode(rowPins[rr], OUTPUT);
      digitalWrite(rowPins[rr], rr == r ? LOW : HIGH);
    }
    delayMicroseconds(20);

    for (int c = 0; c < 4; c++) {
      bool pressedRaw = (digitalRead(colPins[c]) == LOW);
      int idx = r * 4 + c;
      if (pressedRaw != keyRaw[idx]) {
        keyChangeMs[idx] = millis();
        keyRaw[idx] = pressedRaw;
      }
      if ((millis() - keyChangeMs[idx]) > KEY_DEBOUNCE_MS) {
        keyStable[idx] = pressedRaw;
      }
    }
  }

  for (int rr = 0; rr < 4; rr++) digitalWrite(rowPins[rr], HIGH);
}

bool prevStable[16];
bool keyPressedEdge(int idx) {
  bool nowP = keyStable[idx];
  bool edge = nowP && !prevStable[idx];
  return edge;
}
bool keyHeld(int idx) { return keyStable[idx]; }
void latchKeys() {
  for (int i = 0; i < 16; i++) prevStable[i] = keyStable[i];
}

// Geographic coordinate
double OBS_LAT = 0.0000;
double OBS_LON = 0.0000;

struct CoordDigits {
  int sign;
  int intPart[3];
  int fracPart[4];
};

CoordDigits latDigits;
CoordDigits lonDigits;

int coordinatesCursor = 0;
int coordDigitCursor = 0; 

CoordDigits backupLat, backupLon; 

// camera
double fovH = 90.0;              
double fovV = 90.0 * 0.45;      
const double FOV_MIN = 20.0;
const double FOV_MAX = 360.0;
const double FOV_STEP = 10.0;
const double STEP_DEG = 4.0; 

const double MOON_VISIBILITY_FOV = 60.0;

void applyZoom(double delta) {
  fovH += delta;
  if (fovH < FOV_MIN) fovH = FOV_MIN;
  if (fovH > FOV_MAX) fovH = FOV_MAX;
  fovV = fovH * 0.45;
}


enum AppState { STATE_MENU, STATE_TIME_SET, STATE_MAP, STATE_COORDINATES_SET };
AppState state = STATE_MENU;

// clock
struct DateTime { int year, month, day, hour, minute, second; };
DateTime clockTime = {2026, 7, 6, 12, 0, 0};
DateTime clockBackup;
unsigned long lastTickMs = 0;

bool isLeap(int y) { return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0); }
int daysInMonth(int m, int y) {
  static const int d[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  if (m == 2 && isLeap(y)) return 29;
  return d[m-1];
}
void tickClock() {
  unsigned long now = millis();
  if (now - lastTickMs >= 1000) {
    lastTickMs += 1000;
    clockTime.second++;
    if (clockTime.second >= 60) {
      clockTime.second = 0; clockTime.minute++;
      if (clockTime.minute >= 60) {
        clockTime.minute = 0; clockTime.hour++;
        if (clockTime.hour >= 24) {
          clockTime.hour = 0; clockTime.day++;
          if (clockTime.day > daysInMonth(clockTime.month, clockTime.year)) {
            clockTime.day = 1; clockTime.month++;
            if (clockTime.month > 12) { clockTime.month = 1; clockTime.year++; }
          }
        }
      }
    }
  }
}

void syncCoordDigitsFromValues();
void changeTimeField(int delta);
void drawMap();
void drawMenu();
void drawTimeSet();
void plotBody(const char* label, double alt, double az, int top, int h);
void computeAltAz(int idx, double JD, double &alt, double &az, double &dist);

int timeCursor = 0;

// menu
const char* menuItems[] = {"Start", "Time", "Coords"};
const int menuCount = 3;
int menuIndex = 0;

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
      clockBackup = clockTime;
      timeCursor = 0;
      state = STATE_TIME_SET;
    } else if (menuIndex == 2) {
      syncCoordDigitsFromValues();
      backupLat = latDigits;
      backupLon = lonDigits;
      coordinatesCursor = 0;
      coordDigitCursor = 0;
      state = STATE_COORDINATES_SET;
    }
  }
  drawMenu();
}

// coordinates settings
void doubleToDigits(double val, CoordDigits &d) {
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

double digitsToDouble(const CoordDigits &d) {
  int ip = d.intPart[0]*100 + d.intPart[1]*10 + d.intPart[2];
  int fp = d.fracPart[0]*1000 + d.fracPart[1]*100 + d.fracPart[2]*10 + d.fracPart[3];
  double val = ip + fp / 10000.0;
  return d.sign ? -val : val;
}

void syncCoordDigitsFromValues() {
  doubleToDigits(OBS_LAT, latDigits);
  doubleToDigits(OBS_LON, lonDigits);
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

int* getActiveDigitPtr(CoordDigits &d) {
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

void changeCoordinatesField(int delta) {
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

// time settings
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

void changeTimeField(int delta) {
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

double julianDate(const DateTime &t) {
  int Y = t.year, M = t.month;
  double D = t.day + (t.hour + t.minute/60.0 + t.second/3600.0) / 24.0;
  if (M <= 2) { Y -= 1; M += 12; }
  int A = Y / 100;
  int B = 2 - A + A / 4;
  return floor(365.25 * (Y + 4716)) + floor(30.6001 * (M + 1)) + D + B - 1524.5;
}

struct OrbitElem {
  double a, adot, e, edot, I, Idot, L, Ldot, peri, peridot, node, nodedot;
};
OrbitElem elements[8] = {
  {0.38709927,0.00000037, 0.20563593,0.00001906, 7.00497902,-0.00594749, 252.25032350,149472.67411175, 77.45779628,0.16047689, 48.33076593,-0.12534081},
  {0.72333566,0.00000390, 0.00677672,-0.00004107,3.39467605,-0.00078890, 181.97909950,58517.81538729,  131.60246718,0.00268329,76.67984255,-0.27769418},
  {1.00000261,0.00000562, 0.01671123,-0.00004392,-0.00001531,-0.01294668,100.46457166,35999.37244981,  102.93768193,0.32327364,0.0,0.0},
  {1.52371034,0.00001847, 0.09339410,0.00007882, 1.84969142,-0.00813131, -4.55343205,19140.30268499,   -23.94362959,0.44441088,49.55953891,-0.29257343},
  {5.20288700,-0.00011607,0.04838624,-0.00013253,1.30439695,-0.00183714, 34.39644051,3034.74612775,    14.72847983,0.21252668, 100.47390909,0.20469106},
  {9.53667594,-0.00125060,0.05386179,-0.00050991,2.48599187,0.00193609,  49.95424423,1222.49362201,    92.59887831,-0.41897216,113.66242448,-0.28867794},
  {19.18916464,-0.00196176,0.04725744,-0.00004397,0.77263783,-0.00242939,313.23810451,428.48202785,    170.95427630,0.40805281,74.01692503,0.04240589},
  {30.06992276,0.00026291,0.00859048,0.00005105, 1.77004347,0.00035372,  -55.12002969,218.45945325,    44.96476227,-0.32241464,131.78422574,-0.00508664}
};
const char* planetAbbr[8] = {"Me","Ve","Ea","Ma","Ju","Sa","Ur","Ne"};
const char* planetFull[8] = {"Mercury","Venus","Earth","Mars","Jupiter","Saturn","Uranus","Neptune"};

double deg2rad(double d) { return d * M_PI / 180.0; }
double rad2deg(double r) { return r * 180.0 / M_PI; }
double norm360(double d) { d = fmod(d, 360.0); if (d < 0) d += 360.0; return d; }
double norm180(double d) { d = norm360(d); if (d > 180.0) d -= 360.0; return d; }

const double AU_KM = 149597870.7;

void heliocentricPos(int idx, double T, double &x, double &y, double &z) {
  OrbitElem &el = elements[idx];
  double a = el.a + el.adot * T;
  double e = el.e + el.edot * T;
  double I = deg2rad(el.I + el.Idot * T);
  double L = el.L + el.Ldot * T;
  double peri = el.peri + el.peridot * T;
  double node = el.node + el.nodedot * T;
  double M = deg2rad(norm180(L - peri));
  double w = deg2rad(peri - node);
  double Om = deg2rad(node);
  double E = M + e * sin(M);
  for (int i = 0; i < 6; i++) E -= (E - e * sin(E) - M) / (1 - e * cos(E));
  double xOrb = a * (cos(E) - e);
  double yOrb = a * sqrt(1 - e * e) * sin(E);
  double cosO=cos(Om), sinO=sin(Om), cosw=cos(w), sinw=sin(w), cosI=cos(I), sinI=sin(I);
  x = (cosO*cosw - sinO*sinw*cosI)*xOrb + (-cosO*sinw - sinO*cosw*cosI)*yOrb;
  y = (sinO*cosw + cosO*sinw*cosI)*xOrb + (-sinO*sinw + cosO*cosw*cosI)*yOrb;
  z = (sinw*sinI)*xOrb + (cosw*sinI)*yOrb;
}

void equatorialToAltAz(double raRad, double decRad, double JD, double &alt, double &az) {
  double T = (JD - 2451545.0) / 36525.0;
  double d = JD - 2451545.0;
  double gmst = norm360(280.46061837 + 360.98564736629*d + 0.000387933*T*T - (T*T*T)/38710000.0);
  double lst = norm360(gmst + OBS_LON);
  double H = deg2rad(norm180(lst - rad2deg(raRad)));
  double phi = deg2rad(OBS_LAT);

  double sinAlt = sin(decRad)*sin(phi) + cos(decRad)*cos(phi)*cos(H);
  double altR = asin(sinAlt);
  double cosAz = (sin(decRad) - sin(altR)*sin(phi)) / (cos(altR)*cos(phi));
  if (cosAz > 1) cosAz = 1; if (cosAz < -1) cosAz = -1;
  double azDeg = rad2deg(acos(cosAz));
  if (sin(H) > 0) azDeg = 360.0 - azDeg;

  alt = rad2deg(altR);
  az = norm360(azDeg);
}

void computeAltAz(int idx, double JD, double &alt, double &az, double &dist) {
  double T = (JD - 2451545.0) / 36525.0;
  double xE, yE, zE;
  heliocentricPos(2, T, xE, yE, zE);
  double xg, yg, zg;
  if (idx == -1) { xg = -xE; yg = -yE; zg = -zE; }
  else { double xp,yp,zp; heliocentricPos(idx, T, xp, yp, zp); xg = xp-xE; yg = yp-yE; zg = zp-zE; }

  dist = sqrt(xg*xg + yg*yg + zg*zg);

  double eps = deg2rad(23.43929111 - 0.0130042 * T);
  double xeq = xg;
  double yeq = yg*cos(eps) - zg*sin(eps);
  double zeq = yg*sin(eps) + zg*cos(eps);
  double ra = atan2(yeq, xeq);
  double dec = atan2(zeq, sqrt(xeq*xeq + yeq*yeq));

  double d = JD - 2451545.0;
  double gmst = norm360(280.46061837 + 360.98564736629*d + 0.000387933*T*T - (T*T*T)/38710000.0);
  double lst = norm360(gmst + OBS_LON);
  double H = deg2rad(norm180(lst - rad2deg(ra)));
  double phi = deg2rad(OBS_LAT);

  double sinAlt = sin(dec)*sin(phi) + cos(dec)*cos(phi)*cos(H);
  double altR = asin(sinAlt);
  double cosAz = (sin(dec) - sin(altR)*sin(phi)) / (cos(altR)*cos(phi));
  if (cosAz > 1) cosAz = 1; if (cosAz < -1) cosAz = -1;
  double azDeg = rad2deg(acos(cosAz));
  if (sin(H) > 0) azDeg = 360.0 - azDeg;

  alt = rad2deg(altR);
  az = norm360(azDeg);
}

// map

struct MoonData {
  int parentIdx;
  const char* abbr;
  const char* fullName;
  double aKm;
  double periodDays;
};

const int MOON_COUNT = 9;
MoonData moons[MOON_COUNT] = {
  {3, "Ph", "Phobos",    9376.0,    0.318910},
  {3, "De", "Deimos",    23463.0,   1.263000},
  {4, "Io", "Io",        421800.0,  1.769138},
  {4, "Eu", "Europa",    671100.0,  3.551181},
  {4, "Ga", "Ganymede",  1070400.0, 7.154553},
  {4, "Ca", "Callisto",  1882700.0, 16.689020},
  {5, "Ti", "Titan",     1221870.0, 15.945000},
  {6, "Tn", "Titania",   436300.0,  8.706000},
  {7, "Tr", "Triton",    354800.0,  5.877000}
};

// Arbitrary-phase circular orbit angle
double moonOrbitAngleDeg(double periodDays, double JD) {
  const double REF_JD = 2451545.0;
  double cycles = (JD - REF_JD) / periodDays;
  double frac = cycles - floor(cycles);
  return norm360(frac * 360.0);
}

const double MOON_VISUAL_EXAGGERATION = 45.0;

void computeMoonAltAz(int moonIndex, double JD, double planetAlt, double planetAz, double planetDistAU,
                       double &alt, double &az, double &angSepDeg) {
  MoonData &m = moons[moonIndex];
  double theta = deg2rad(moonOrbitAngleDeg(m.periodDays, JD));
  double distKm = planetDistAU * AU_KM;
  double angRadiusDeg = rad2deg(m.aKm / distKm) * MOON_VISUAL_EXAGGERATION;
  angSepDeg = angRadiusDeg;
  alt = planetAlt + angRadiusDeg * sin(theta);
  az  = planetAz  + angRadiusDeg * cos(theta);
}

// Luna / earth moon
void computeEarthMoonAltAz(double JD, double &alt, double &az, double &dist) {
  double T = (JD - 2451545.0) / 36525.0;

  double lonMoon = norm360(218.3164477 + 481267.88123421 * T);
  double lam = deg2rad(lonMoon);
  double eps = deg2rad(23.43929111 - 0.0130042 * T);

  double xeq = cos(lam);
  double yeq = cos(eps) * sin(lam);
  double zeq = sin(eps) * sin(lam);

  double ra = atan2(yeq, xeq);
  double dec = asin(zeq);

  equatorialToAltAz(ra, dec, JD, alt, az);
  dist = 384400.0 / AU_KM; 
}

// map

double camAz = 180.0, camAlt = 30.0;

enum LockType { LOCK_NONE, LOCK_SUN, LOCK_PLANET, LOCK_EARTHMOON, LOCK_SAT_MOON };
LockType lockedType = LOCK_NONE;
int lockedIndex = -1;

void plotBody(const char* label, double alt, double az, int top, int h) {
  double dAz = norm180(az - camAz);
  double dAlt = alt - camAlt;
  if (fabs(dAz) > fovH/2.0 || fabs(dAlt) > fovV/2.0) return;
  int sx = (int)(SCREEN_WIDTH/2.0 + (dAz/(fovH/2.0)) * (SCREEN_WIDTH/2.0));
  int sy = top + (int)(h/2.0 - (dAlt/(fovV/2.0)) * (h/2.0));
  display.fillCircle(sx, sy, 2, SSD1306_WHITE);
  display.setCursor(sx + 3, sy - 4);
  display.print(label);
}

void plotMoon(const char* label, double alt, double az, int top, int h) {
  double dAz = norm180(az - camAz);
  double dAlt = alt - camAlt;
  if (fabs(dAz) > fovH/2.0 || fabs(dAlt) > fovV/2.0) return;
  int sx = (int)(SCREEN_WIDTH/2.0 + (dAz/(fovH/2.0)) * (SCREEN_WIDTH/2.0));
  int sy = top + (int)(h/2.0 - (dAlt/(fovV/2.0)) * (h/2.0));
  display.drawPixel(sx, sy, SSD1306_WHITE);
  display.drawPixel(sx+1, sy, SSD1306_WHITE);
  display.drawPixel(sx, sy+1, SSD1306_WHITE);
  display.setCursor(sx + 3, sy - 4);
  display.print(label);
}

void tryLockNearest(double JD) {
  LockType bestType = LOCK_NONE;
  int bestIndex = -1;
  double bestDist2 = 1e9;

  // Sun
  {
    double alt, az, dist;
    computeAltAz(-1, JD, alt, az, dist);
    double dAz = norm180(az - camAz);
    double dAlt = alt - camAlt;
    if (fabs(dAz) <= fovH/2.0 && fabs(dAlt) <= fovV/2.0) {
      double d2 = dAz*dAz + dAlt*dAlt;
      if (d2 < bestDist2) { bestDist2 = d2; bestType = LOCK_SUN; bestIndex = -1; }
    }
  }

  // Planets
  double planetAlt[8], planetAz[8], planetDist[8];
  for (int i = 0; i < 8; i++) {
    if (i == 2) continue;
    computeAltAz(i, JD, planetAlt[i], planetAz[i], planetDist[i]);
    double dAz = norm180(planetAz[i] - camAz);
    double dAlt = planetAlt[i] - camAlt;
    if (fabs(dAz) <= fovH/2.0 && fabs(dAlt) <= fovV/2.0) {
      double d2 = dAz*dAz + dAlt*dAlt;
      if (d2 < bestDist2) { bestDist2 = d2; bestType = LOCK_PLANET; bestIndex = i; }
    }
  }

  // Earth's Moon
  {
    double alt, az, dist;
    computeEarthMoonAltAz(JD, alt, az, dist);
    double dAz = norm180(az - camAz);
    double dAlt = alt - camAlt;
    if (fabs(dAz) <= fovH/2.0 && fabs(dAlt) <= fovV/2.0) {
      double d2 = dAz*dAz + dAlt*dAlt;
      if (d2 < bestDist2) { bestDist2 = d2; bestType = LOCK_EARTHMOON; bestIndex = -1; }
    }
  }

  // Satellite moons
  if (fovH < MOON_VISIBILITY_FOV) {
    for (int m = 0; m < MOON_COUNT; m++) {
      int p = moons[m].parentIdx;
      double alt, az, angSep;
      computeMoonAltAz(m, JD, planetAlt[p], planetAz[p], planetDist[p], alt, az, angSep);
      double dAz = norm180(az - camAz);
      double dAlt = alt - camAlt;
      if (fabs(dAz) <= fovH/2.0 && fabs(dAlt) <= fovV/2.0) {
        double d2 = dAz*dAz + dAlt*dAlt;
        if (d2 < bestDist2) { bestDist2 = d2; bestType = LOCK_SAT_MOON; bestIndex = m; }
      }
    }
  }

  if (bestType != LOCK_NONE) {
    lockedType = bestType;
    lockedIndex = bestIndex;
  }
}

void getLockedAltAz(double JD, double &alt, double &az, double &dist) {
  switch (lockedType) {
    case LOCK_SUN:
      computeAltAz(-1, JD, alt, az, dist);
      break;
    case LOCK_PLANET:
      computeAltAz(lockedIndex, JD, alt, az, dist);
      break;
    case LOCK_EARTHMOON:
      computeEarthMoonAltAz(JD, alt, az, dist);
      break;
    case LOCK_SAT_MOON: {
      int p = moons[lockedIndex].parentIdx;
      double pAlt, pAz, pDist, angSep;
      computeAltAz(p, JD, pAlt, pAz, pDist);
      computeMoonAltAz(lockedIndex, JD, pAlt, pAz, pDist, alt, az, angSep);
      dist = pDist; 
      break;
    }
    default:
      alt = camAlt; az = camAz; dist = 0;
      break;
  }
}

void drawMap() {
  display.clearDisplay();
  double JD = julianDate(clockTime);

  if (lockedType != LOCK_NONE) {
    double alt, az, dist;
    getLockedAltAz(JD, alt, az, dist);
    camAz = az; camAlt = alt;
  }

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("AZ:"); display.print((int)camAz);
  display.print(" ALT:"); display.print((int)camAlt);
  display.print(" FOV:"); display.print((int)fovH);

  const int top = 9;
  const int h = SCREEN_HEIGHT - top;
  const int cx = SCREEN_WIDTH / 2;
  const int cy = top + h / 2;

  {
    double dAlt = 0.0 - camAlt;
    if (fabs(dAlt) < fovV/2.0) {
      int ly = top + (int)(h/2.0 - (dAlt/(fovV/2.0))*(h/2.0));
      display.drawLine(0, ly, 127, ly, SSD1306_WHITE);
    }
  }

  // Sun
  {
    double alt, az, dist;
    computeAltAz(-1, JD, alt, az, dist);
    plotBody("Su", alt, az, top, h);
  }

  double planetAlt[8], planetAz[8], planetDist[8];
  for (int i = 0; i < 8; i++) {
    if (i == 2) continue;
    computeAltAz(i, JD, planetAlt[i], planetAz[i], planetDist[i]);
    plotBody(planetAbbr[i], planetAlt[i], planetAz[i], top, h);
  }

  if (fovH < MOON_VISIBILITY_FOV) {
    for (int m = 0; m < MOON_COUNT; m++) {
      int p = moons[m].parentIdx;
      double alt, az, angSep;
      computeMoonAltAz(m, JD, planetAlt[p], planetAz[p], planetDist[p], alt, az, angSep);
      plotMoon(moons[m].abbr, alt, az, top, h);
    }
  }

  {
    double alt, az, dist;
    computeEarthMoonAltAz(JD, alt, az, dist);
    plotBody("Mo", alt, az, top, h);
  }

  display.drawLine(cx - 4, cy, cx + 4, cy, SSD1306_WHITE);
  display.drawLine(cx, cy - 4, cx, cy + 4, SSD1306_WHITE);

  if (lockedType != LOCK_NONE) {
    double alt, az, dist;
    getLockedAltAz(JD, alt, az, dist);

    const char* name;
    const char* kind;
    switch (lockedType) {
      case LOCK_SUN:       name = "Sun";                        kind = "star";    break;
      case LOCK_PLANET:    name = planetFull[lockedIndex];      kind = "planet";  break;
      case LOCK_EARTHMOON: name = "Moon";                       kind = "moon";    break;
      case LOCK_SAT_MOON:  name = moons[lockedIndex].fullName;  kind = "moon";    break;
      default:             name = "";                           kind = "";        break;
    }

    display.setCursor(cx + 6, cy - 10);
    display.print(name);
    display.setCursor(cx + 6, cy + 2);
    display.print(kind);
    display.setCursor(cx + 6, cy + 12);
    if (lockedType == LOCK_EARTHMOON) {
      display.print((long)(dist * AU_KM));
      display.print("km");
    } else {
      display.print(dist, 2);
      display.print("a.u.");
    }
  }

  display.display();
}

void handleMap() {
  double JD = julianDate(clockTime);

  if (lockedType == LOCK_NONE) {
    if (keyPressedEdge(KEY_LEFT))  camAz = norm360(camAz - STEP_DEG);
    if (keyPressedEdge(KEY_RIGHT)) camAz = norm360(camAz + STEP_DEG);
    if (keyPressedEdge(KEY_UP))    { camAlt += STEP_DEG; if (camAlt > 89) camAlt = 89; }
    if (keyPressedEdge(KEY_DOWN))  { camAlt -= STEP_DEG; if (camAlt < -20) camAlt = -20; }
  }

  if (keyPressedEdge(KEY_ZOOM_IN))  applyZoom(-FOV_STEP);
  if (keyPressedEdge(KEY_ZOOM_OUT)) applyZoom(+FOV_STEP);

  if (keyPressedEdge(KEY_OK)) {
    if (lockedType == LOCK_NONE) {
      tryLockNearest(JD);
    }
  }

  if (keyPressedEdge(KEY_BACK)) {
    if (lockedType != LOCK_NONE) {
      lockedType = LOCK_NONE;
      lockedIndex = -1;
    } else {
      state = STATE_MENU;
      return;
    }
  }

  drawMap();
}


void setup() {
  Serial.begin(115200);

  for (int i = 0; i < 4; i++) pinMode(colPins[i], INPUT_PULLUP);
  for (int i = 0; i < 4; i++) { pinMode(rowPins[i], OUTPUT); digitalWrite(rowPins[i], HIGH); }
  for (int i = 0; i < 16; i++) { keyRaw[i]=false; keyStable[i]=false; prevStable[i]=false; keyChangeMs[i]=0; }

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
    case STATE_MAP:             handleMap();           break;
    case STATE_COORDINATES_SET: handleCoordinateSet(); break;
  }

  latchKeys();
  delay(15);
}
