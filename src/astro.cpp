#include <math.h>
#include "astro.h"

double OBS_LAT = 0.0;
double OBS_LON = 0.0;

const char* planetAbbr[8] = {"Me","Ve","Ea","Ma","Ju","Sa","Ur","Ne"};
const char* planetFull[8] = {"Mercury","Venus","Earth","Mars","Jupiter","Saturn","Uranus","Neptune"};

double deg2rad(double d) { return d * M_PI / 180.0; }
double rad2deg(double r) { return r * 180.0 / M_PI; }
double norm360(double d) { d = fmod(d, 360.0); if (d < 0) d += 360.0; return d; }
double norm180(double d) { d = norm360(d); if (d > 180.0) d -= 360.0; return d; }

struct OrbitElem {
  double a, adot, e, edot, I, Idot, L, Ldot, peri, peridot, node, nodedot;
};

static OrbitElem elements[8] = {
  {0.38709927,0.00000037, 0.20563593,0.00001906, 7.00497902,-0.00594749, 252.25032350,149472.67411175, 77.45779628,0.16047689, 48.33076593,-0.12534081},
  {0.72333566,0.00000390, 0.00677672,-0.00004107,3.39467605,-0.00078890, 181.97909950,58517.81538729,  131.60246718,0.00268329,76.67984255,-0.27769418},
  {1.00000261,0.00000562, 0.01671123,-0.00004392,-0.00001531,-0.01294668,100.46457166,35999.37244981,  102.93768193,0.32327364,0.0,0.0},
  {1.52371034,0.00001847, 0.09339410,0.00007882, 1.84969142,-0.00813131, -4.55343205,19140.30268499,   -23.94362959,0.44441088,49.55953891,-0.29257343},
  {5.20288700,-0.00011607,0.04838624,-0.00013253,1.30439695,-0.00183714, 34.39644051,3034.74612775,    14.72847983,0.21252668, 100.47390909,0.20469106},
  {9.53667594,-0.00125060,0.05386179,-0.00050991,2.48599187,0.00193609,  49.95424423,1222.49362201,    92.59887831,-0.41897216,113.66242448,-0.28867794},
  {19.18916464,-0.00196176,0.04725744,-0.00004397,0.77263783,-0.00242939,313.23810451,428.48202785,    170.95427630,0.40805281,74.01692503,0.04240589},
  {30.06992276,0.00026291,0.00859048,0.00005105, 1.77004347,0.00035372,  -55.12002969,218.45945325,    44.96476227,-0.32241464,131.78422574,-0.00508664}
};

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

  // Kepler's equation, solved by Newton's method.
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
  heliocentricPos(2, T, xE, yE, zE); // Earth

  double xg, yg, zg;
  if (idx == -1) {

    xg = -xE; yg = -yE; zg = -zE;
  } else {
    double xp, yp, zp;
    heliocentricPos(idx, T, xp, yp, zp);
    xg = xp - xE; yg = yp - yE; zg = zp - zE;
  }

  dist = sqrt(xg*xg + yg*yg + zg*zg);

  double eps = deg2rad(23.43929111 - 0.0130042 * T);
  double xeq = xg;
  double yeq = yg*cos(eps) - zg*sin(eps);
  double zeq = yg*sin(eps) + zg*cos(eps);
  double ra = atan2(yeq, xeq);
  double dec = atan2(zeq, sqrt(xeq*xeq + yeq*yeq));

  equatorialToAltAz(ra, dec, JD, alt, az);
}
