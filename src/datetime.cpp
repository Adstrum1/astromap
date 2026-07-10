#include <Arduino.h>
#include <math.h>
#include "datetime.h"

DateTime clockTime = {2026, 7, 6, 12, 0, 0};
DateTime clockBackup;
unsigned long lastTickMs = 0;

bool isLeap(int y) {
  return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

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

double julianDate(const DateTime &t) {
  int Y = t.year, M = t.month;
  double D = t.day + (t.hour + t.minute/60.0 + t.second/3600.0) / 24.0;
  if (M <= 2) { Y -= 1; M += 12; }
  int A = Y / 100;
  int B = 2 - A + A / 4;
  return floor(365.25 * (Y + 4716)) + floor(30.6001 * (M + 1)) + D + B - 1524.5;
}
