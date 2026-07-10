#include <math.h>
#include "app_state.h"
#include "config.h"
#include "keypad.h"
#include "datetime.h"
#include "astro.h"
#include "moons.h"
#include "map_screen.h"

static double camAz = 180.0, camAlt = 30.0;

static double fovH = 90.0;
static double fovV = 90.0 * 0.45;

enum LockType { LOCK_NONE, LOCK_SUN, LOCK_PLANET, LOCK_EARTHMOON, LOCK_SAT_MOON };
static LockType lockedType = LOCK_NONE;
static int lockedIndex = -1;

static void plotBody(const char* label, double alt, double az, int top, int h, bool drawLabel) {
  double dAz = norm180(az - camAz);
  double dAlt = alt - camAlt;
  if (fabs(dAz) > fovH/2.0 || fabs(dAlt) > fovV/2.0) return;
  int sx = (int)(SCREEN_WIDTH/2.0 + (dAz/(fovH/2.0)) * (SCREEN_WIDTH/2.0));
  int sy = top + (int)(h/2.0 - (dAlt/(fovV/2.0)) * (h/2.0));
  display.fillCircle(sx, sy, 2, SSD1306_WHITE);
  if (drawLabel) {
    display.setCursor(sx + 3, sy - 4);
    display.print(label);
  }
}

static void plotMoon(const char* label, double alt, double az, int top, int h, bool drawLabel) {
  double dAz = norm180(az - camAz);
  double dAlt = alt - camAlt;
  if (fabs(dAz) > fovH/2.0 || fabs(dAlt) > fovV/2.0) return;
  int sx = (int)(SCREEN_WIDTH/2.0 + (dAz/(fovH/2.0)) * (SCREEN_WIDTH/2.0));
  int sy = top + (int)(h/2.0 - (dAlt/(fovV/2.0)) * (h/2.0));
  display.drawPixel(sx, sy, SSD1306_WHITE);
  display.drawPixel(sx+1, sy, SSD1306_WHITE);
  display.drawPixel(sx, sy+1, SSD1306_WHITE);
  if (drawLabel) {
    display.setCursor(sx + 3, sy - 4);
    display.print(label);
  }
}



static void applyZoom(double delta) {
  fovH += delta;
  if (fovH < FOV_MIN) fovH = FOV_MIN;
  if (fovH > FOV_MAX) fovH = FOV_MAX;
  fovV = fovH * 0.45;
}

static void tryLockNearest(double JD) {
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
    if (i == 2) continue; // Earth (the observer)
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

// Returns the current Alt/Az/dist(AU) of whatever is currently locked.
static void getLockedAltAz(double JD, double &alt, double &az, double &dist) {
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

  bool showLabels = (lockedType == LOCK_NONE);

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

  // Horizon line (maybe i should delete it)
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
    plotBody("Su", alt, az, top, h, showLabels);
  }

  // Planets
  double planetAlt[8], planetAz[8], planetDist[8];
  for (int i = 0; i < 8; i++) {
    if (i == 2) continue;
    computeAltAz(i, JD, planetAlt[i], planetAz[i], planetDist[i]);
    plotBody(planetAbbr[i], planetAlt[i], planetAz[i], top, h, showLabels);
  }

  if (fovH < MOON_VISIBILITY_FOV) {
    for (int m = 0; m < MOON_COUNT; m++) {
      int p = moons[m].parentIdx;
      double alt, az, angSep;
      computeMoonAltAz(m, JD, planetAlt[p], planetAz[p], planetDist[p], alt, az, angSep);
      plotMoon(moons[m].abbr, alt, az, top, h, showLabels);
    }
  }
  // Earth's Moon
  {
    double alt, az, dist;
    computeEarthMoonAltAz(JD, alt, az, dist);
    plotBody("Mo", alt, az, top, h, showLabels);
  }

  // Crosshair
  display.drawLine(cx - 4, cy, cx + 4, cy, SSD1306_WHITE);
  display.drawLine(cx, cy - 4, cx, cy + 4, SSD1306_WHITE);

  // Info panel
  if (lockedType != LOCK_NONE) {
    double alt, az, dist;
    getLockedAltAz(JD, alt, az, dist);

    const char* name;
    const char* kind;
    switch (lockedType) {
      case LOCK_SUN:       name = "Sun";                       kind = "star";   break;
      case LOCK_PLANET:    name = planetFull[lockedIndex];      kind = "planet"; break;
      case LOCK_EARTHMOON: name = "Moon";                       kind = "moon";   break;
      case LOCK_SAT_MOON:  name = moons[lockedIndex].fullName;  kind = "moon";   break;
      default:             name = "";                           kind = "";       break;
    }

    display.setCursor(cx + 6, cy - 18);
    display.print(name);

    display.setCursor(cx + 6, cy - 8);
    display.print(kind);

    display.setCursor(cx + 6, cy + 2);
    display.print("Az"); display.print((int)az);
    display.print(" Al"); display.print((int)alt);

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
