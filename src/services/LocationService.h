#pragma once
#include <Arduino.h>

class Settings;
class NetClient;
class EventBus;
class TimeService;

// services/LocationService — the active observer location every astro tab + the
// Director use (spec §4, §6 Location, §13). Modes: Auto (IP geolocation, the
// default), a saved preset, or GPS (absent for now). Resolving Auto also yields
// a timezone offset, which it feeds straight to TimeService.
struct GeoLocation {
  double lat = 0;
  double lon = 0;
  long   tzOffsetSec = 0;
  String name;
  bool   valid = false;
};

class LocationService {
public:
  bool begin(Settings* s, NetClient* net, EventBus* bus, TimeService* time);
  void refresh();                       // re-resolve per the current mode
  const GeoLocation& active() const { return _loc; }

private:
  void refreshAuto();                   // IP geolocation (keyless)
  void applyAndPublish(const GeoLocation& loc, bool persist);

  Settings*    _s    = nullptr;
  NetClient*   _net  = nullptr;
  EventBus*    _bus  = nullptr;
  TimeService* _time = nullptr;
  GeoLocation  _loc;
};
