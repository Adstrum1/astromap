#pragma once

struct DateTime {
  int year, month, day, hour, minute, second;
};

extern DateTime clockTime;
extern DateTime clockBackup;
extern unsigned long lastTickMs;

bool isLeap(int y);
int daysInMonth(int m, int y);

void tickClock();

double julianDate(const DateTime &t);
