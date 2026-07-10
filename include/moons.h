#pragma once

struct MoonData {
  int parentIdx;
  const char* abbr;
  const char* fullName;
  double aKm;
  double periodDays;  
};

extern MoonData moons[];
extern const int MOON_COUNT;


double moonOrbitAngleDeg(double periodDays, double JD);

void computeMoonAltAz(int moonIndex, double JD, double planetAlt, double planetAz, double planetDistAU,
                       double &alt, double &az, double &angSepDeg);

void computeEarthMoonAltAz(double JD, double &alt, double &az, double &dist);
