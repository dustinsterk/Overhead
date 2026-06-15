#pragma once
#include <Arduino.h>

class TimeService;
class LocationService;
class Display;
class Settings;

// core/ThemeController — drives the global palette + backlight from Sun altitude
// (spec §7.9), independent of the Director's attention logic. Auto mode flips
// day<->night with hysteresis so it won't flicker at the threshold; night uses
// the plain-dark or red dark-adapt palette and dims the backlight.
class ThemeController {
public:
  void begin(TimeService* time, LocationService* loc, Display* display, Settings* settings);
  void tick(uint32_t nowMs);     // re-evaluates every ~15 s
  bool isNight() const { return _night; }

private:
  void apply(bool night);

  TimeService*     _time = nullptr;
  LocationService* _loc = nullptr;
  Display*         _display = nullptr;
  Settings*        _settings = nullptr;

  bool     _night = false;
  bool     _applied = false;
  uint32_t _lastMs = 0;
};
