#pragma once
#include "Coords.h"
#include "Time.h"
#include <math.h>

// astro/SkyProjection — shared azimuthal sky-dome projection used by every page
// that draws a real star chart (Star Map, the clock's sky background, etc.).
// Zenith sits at the centre, the horizon at radius R; azimuth from North,
// increasing eastward. Returns false when the point is below the horizon.
namespace astro {

inline bool projectSky(double raHours, double decDeg, double latRad, double lst,
                       int cx, int cy, int R, int& sx, int& sy, float& alt) {
  Equatorial eq{ raHours * 15.0 * DEG2RAD, decDeg * DEG2RAD };
  Horizontal h = equatorialToHorizontal(eq, latRad, lst);
  alt = (float)(h.altRad * RAD2DEG);
  if (alt <= 0) return false;
  double rr = R * (90.0 - alt) / 90.0;
  sx = cx + (int)lround(rr * sin(h.azRad));
  sy = cy - (int)lround(rr * cos(h.azRad));
  return true;
}

} // namespace astro
