#pragma once
#include <stdint.h>

class Display;

// hal/Touch — XPT2046 read + calibration (spec §4 hal/Touch).
//
// Calibration is the 8-value affine map LovyanGFX produces from a 4-corner
// crosshair routine. It is persisted to LittleFS so first boot calibrates once
// and later boots load it; Health/Settings can force a recalibration later
// (spec §4.1 "re-runnable from Health").
class Touch {
public:
  // Load saved calibration if present; otherwise run the interactive 4-corner
  // routine and persist it. Returns true once touch is usable.
  bool begin(Display& display);

  // Force the interactive routine and overwrite the saved calibration.
  bool calibrate(Display& display);

  // True if currently pressed; writes display-space coords into x,y.
  bool read(Display& display, int16_t& x, int16_t& y);

private:
  static constexpr const char* kCalPath = "/touch_cal.dat";
  bool loadCalibration(uint16_t out[8]);
  void saveCalibration(const uint16_t data[8]);
};
