#pragma once
#include <Arduino.h>
#include <vector>
#include "TleProvider.h"   // ProviderStatus

class Settings;
class NetClient;
class Cache;
class EventBus;
class LocationService;

// providers/WeatherProvider — Open-Meteo hourly cloud cover + precip probability
// for observability (spec §6 Agenda, §9; no key). Feeds the Agenda Sky Window and
// the Director's cloud gating. Hourly arrays indexed from a UTC base epoch.
class WeatherProvider {
public:
  void begin(Settings* s, NetClient* net, Cache* cache, EventBus* bus, LocationService* loc);
  void refresh(bool force = false);

  int cloudCoverAt(time_t t) const;     // 0..100, -1 unknown
  int precipProbAt(time_t t) const;     // 0..100, -1 unknown
  bool valid() const { return !_cloud.empty(); }
  ProviderStatus status() const { return _status; }
  uint32_t lastFetched() const { return _lastFetched; }

private:
  bool parse(const String& body);

  Settings*        _s = nullptr;
  NetClient*       _net = nullptr;
  Cache*           _cache = nullptr;
  EventBus*        _bus = nullptr;
  LocationService* _loc = nullptr;

  time_t _base = 0;                      // epoch of hourly[0]
  std::vector<int8_t> _cloud;
  std::vector<int8_t> _precip;
  ProviderStatus _status = ProviderStatus::Loading;
  uint32_t _lastFetched = 0;
  bool _inflight = false;
};
