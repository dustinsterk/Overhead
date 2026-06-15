#include "ThemeController.h"
#include "Theme.h"
#include "../hal/Display.h"
#include "../services/TimeService.h"
#include "../services/LocationService.h"
#include "../services/Settings.h"
#include "../astro/Sun.h"

void ThemeController::begin(TimeService* time, LocationService* loc, Display* display, Settings* settings) {
  _time = time; _loc = loc; _display = display; _settings = settings;
  apply(false);                 // start in day until the first evaluation
}

void ThemeController::tick(uint32_t nowMs) {
  if (_applied && nowMs - _lastMs < 15000) return;
  _lastMs = nowMs;

  String mode = _settings->getString("themeMode", "auto");
  bool night;
  if (mode == "day")        night = false;
  else if (mode == "night") night = true;
  else {                                          // auto, with hysteresis
    if (!_time->synced() || !_loc->active().valid) { apply(false); return; }
    double alt = astro::sunAltitudeDeg(_time->julianDate(),
                                       _loc->active().lat, _loc->active().lon);
    double thr = (double)_settings->getInt("themeNightAlt", -6);
    night = _night ? (alt < thr + 2.0)            // -> day only once well above
                   : (alt < thr);                 // -> night once below
  }
  apply(night);
}

void ThemeController::apply(bool night) {
  if (_applied && night == _night) return;
  _night = night;
  _applied = true;

  if (!night) {
    gTheme = themes::dark;
    if (_display) _display->setBacklight(255);
  } else {
    bool red = _settings && _settings->getString("nightPalette", "dark") == "red";
    gTheme = red ? themes::redNight : themes::dark;
    int bl = _settings ? (int)_settings->getInt("nightBacklight", 90) : 90;
    if (_display) _display->setBacklight((uint8_t)constrain(bl, 10, 255));
  }
}
