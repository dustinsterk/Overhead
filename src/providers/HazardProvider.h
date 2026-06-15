#pragma once
#include <Arduino.h>
#include <vector>
#include "TleProvider.h"   // ProviderStatus

class Settings;
class NetClient;
class Cache;
class EventBus;
class LocationService;

// providers/HazardProvider — AIRMET/SIGMET + PIREP near the observer (spec §14,
// aviation phase 2b). NOAA AWC data API (no key). AIRMET/SIGMET filtered by a
// bounding-box-overlap proximity test (cheap, not true point-in-polygon); PIREPs
// by bbox. Produces short hazard lines for the Aviation tab's Hazards view.
struct Hazard {
  String text;
  bool   pirep = false;
};

class HazardProvider {
public:
  void begin(Settings* s, NetClient* net, Cache* cache, EventBus* bus, LocationService* loc);
  void refresh(bool force = false);

  const std::vector<Hazard>& hazards() const { return _all; }
  ProviderStatus status() const { return _status; }
  uint32_t lastFetched() const { return _lastFetched; }

private:
  void fetchAirsig();
  void fetchPirep();
  void rebuild();

  Settings*        _s = nullptr;
  NetClient*       _net = nullptr;
  Cache*           _cache = nullptr;
  EventBus*        _bus = nullptr;
  LocationService* _loc = nullptr;

  std::vector<Hazard> _airsig, _pirep, _all;
  ProviderStatus _status = ProviderStatus::Loading;
  uint32_t _lastFetched = 0;
  bool _inAir = false, _inPi = false;
};
