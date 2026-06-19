#include "Coords.h"
#include "Time.h"
#include <math.h>

namespace astro {

Horizontal equatorialToHorizontal(const Equatorial& eq, double latRad, double lstRad) {
  double ha = lstRad - eq.raRad;                 // hour angle
  double sinAlt = sin(eq.decRad) * sin(latRad)
                + cos(eq.decRad) * cos(latRad) * cos(ha);
  double alt = asin(fmax(-1.0, fmin(1.0, sinAlt)));

  // Azimuth from North, eastward.
  double y = -cos(eq.decRad) * cos(latRad) * sin(ha);
  double x =  sin(eq.decRad) - sin(latRad) * sinAlt;
  double az = atan2(y, x);
  return { wrapTwoPi(az), alt };
}


} // namespace astro
