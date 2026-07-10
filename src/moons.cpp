#include <math.h>
#include "astro.h"
#include "moons.h"

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

static const double MOON_VISUAL_EXAGGERATION = 45.0;

double moonOrbitAngleDeg(double periodDays, double JD) {
  const double REF_JD = 2451545.0;
  double cycles = (JD - REF_JD) / periodDays;
  double frac = cycles - floor(cycles);
  return norm360(frac * 360.0);
}

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
