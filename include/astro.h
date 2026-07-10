#pragma once

const double AU_KM = 149597870.7;

extern double OBS_LAT;
extern double OBS_LON;

extern const char* planetAbbr[8];
extern const char* planetFull[8];

double deg2rad(double d);
double rad2deg(double r);
double norm360(double d);
double norm180(double d);

void heliocentricPos(int idx, double T, double &x, double &y, double &z);

void equatorialToAltAz(double raRad, double decRad, double JD, double &alt, double &az);

void computeAltAz(int idx, double JD, double &alt, double &az, double &dist);
